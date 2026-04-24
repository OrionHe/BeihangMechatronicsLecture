#include "xusb.h"
#include "main.h"
#include "my_config.h"
#include "usbd_cdc_if.h"

#define HOME_SEARCH_VEL_MM_S 10.0f

static bool s_axis_homing[2] = {false, false};

/**
 * @brief  校验和计算（XOR）
 */
static inline uint8_t usb_cmd_calc_checksum(uint8_t cmd, uint8_t data_len, const uint8_t *data)
{
    uint8_t checksum = cmd ^ data_len;
    for (int i = 0; i < data_len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief  打包并发送应答帧
 * 
 * 使用示例：
 * uint8_t status_data[10];
 * // 填充状态数据...
 * usb_send_response(CMD_STATUS_RESPONSE, status_data, 10);
 */
void usb_send_response(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    /* 
     * 帧格式: [HEADER] [CMD] [LEN] [DATA...] [CHECKSUM] [TAIL]
     * 需要通过 CDC_Transmit_FS 发送
     */
    uint8_t tx_buf[256];
    uint8_t checksum = usb_cmd_calc_checksum(cmd, data_len, (uint8_t*)data);
    
    int idx = 0;
    tx_buf[idx++] = FRAME_HEADER;
    tx_buf[idx++] = cmd;
    tx_buf[idx++] = data_len;
    
    if (data_len > 0) {
        memcpy(&tx_buf[idx], data, data_len);
        idx += data_len;
    }
    
    tx_buf[idx++] = checksum;
    tx_buf[idx++] = FRAME_TAIL;
    
    CDC_Transmit_FS(tx_buf, idx);
    /* 外部调用：CDC_Transmit_FS(tx_buf, idx); */
}

int usb_parse_command(const uint8_t *frame, uint16_t frame_len,
                      uint8_t *cmd, uint8_t **data, uint8_t *data_len)
{
    if ((frame == nullptr) || (cmd == nullptr) || (data == nullptr) || (data_len == nullptr)) {
        return 0;
    }

    if (frame_len < 5) return 0;
    
    if (frame[0] != FRAME_HEADER || frame[frame_len-1] != FRAME_TAIL) {
        return 0;
    }
    
    *cmd = frame[1];
    *data_len = frame[2];
    if (frame_len != (uint16_t)(*data_len) + 5U) {
        return 0;
    }
    *data = (uint8_t*)&frame[3];
    
    /* 校验校验和 */
    uint8_t expected_checksum = usb_cmd_calc_checksum(*cmd, *data_len, *data);
    uint8_t actual_checksum = frame[3 + *data_len];
    
    if (expected_checksum != actual_checksum) {
        return 0;
    }
    
    return 1;
}

void usb_handle_command(uint8_t cmd, uint8_t *data, uint8_t data_len)
{
    if ((data == nullptr) || (data_len == 0U)) {
        return;
    }

    uint8_t axis_id = data[0];
    HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);

    auto is_axis_valid = [](uint8_t id) {
        return (id == AXIS_X) || (id == AXIS_Y) || (id == AXIS_ALL);
    };

    auto home_axis = [](x_linear_module::LinearModule &m) {
        m.SetMode(x_linear_module::MODULE_MODE_VELOCITY);
        m.SetTargetVelocity(-HOME_SEARCH_VEL_MM_S);
    };

    auto stop_axis = [](x_linear_module::LinearModule &m) {
        m.SetTargetVelocityHard(0.0f);
    };

    auto set_homing_flag = [](uint8_t id, bool homing) {
        if (id == AXIS_X || id == AXIS_ALL) {
            s_axis_homing[0] = homing;
        }
        if (id == AXIS_Y || id == AXIS_ALL) {
            s_axis_homing[1] = homing;
        }
    };

    auto step_vel_to_mm_s = [](const x_linear_module::LinearModule &m) -> float {
        float step_vel =  m.stepper.step_current_velocity ;
        return step_vel * m.lead * m.stepper.step_angle /
               (m.stepper.step_division * 360.0f);
    };

    auto get_axis_status = [](x_linear_module::LinearModule &m, bool &homing_flag) -> uint8_t {
        if (m.mode == x_linear_module::MODULE_MODE_ERROR) {
            homing_flag = false;
            return STATUS_ERROR;
        }

        if (homing_flag) {
            if ((m.mode == x_linear_module::MODULE_MODE_POSITION) &&
                (m.stepper.step_current_velocity == 0)) {
                homing_flag = false;
                return STATUS_IDLE;
            }
            return STATUS_HOMING;
        }

        if ((m.mode == x_linear_module::MODULE_MODE_POSITION) &&
            (m.stepper.step_current_velocity == 0)) {
            return STATUS_IDLE;
        }

        return (m.stepper.step_current_velocity != 0) ? STATUS_MOVING : STATUS_IDLE;
    };

    if (!is_axis_valid(axis_id)) {
        return;
    }

    switch (cmd)
    {
        case CMD_HOME:
            /* [axis_id] */
            if (axis_id == AXIS_X || axis_id == AXIS_ALL) {
                home_axis(g_linearModule[0]);
            }
            if (axis_id == AXIS_Y || axis_id == AXIS_ALL) {
                home_axis(g_linearModule[1]);
            }
            set_homing_flag(axis_id, true);
            break;
        
        case CMD_MOVE_ABS:
            /* [axis_id][position(float)][speed(float)] */
            if (data_len >= 9U) {
                float position = 0.0f;
                float speed = 0.0f;
                memcpy(&position, &data[1], 4);
                memcpy(&speed, &data[5], 4);

                if (axis_id == AXIS_X) {
                    g_linearModule[0].SetMode(x_linear_module::MODULE_MODE_POSITION);
                    g_linearModule[0].SetTargetPositionWithVelocity(position, speed);
                } else if (axis_id == AXIS_Y) {
                    g_linearModule[1].SetMode(x_linear_module::MODULE_MODE_POSITION);
                    g_linearModule[1].SetTargetPositionWithVelocity(position, speed);
                }
                set_homing_flag(axis_id, false);
            }
            break;
        
        case CMD_SET_VELOCITY:
            /* [axis_id][velocity(float)] */
            if (data_len >= 5U) {
                float velocity = 0.0f;
                memcpy(&velocity, &data[1], 4);

                if (axis_id == AXIS_X) {
                    g_linearModule[0].SetMode(x_linear_module::MODULE_MODE_VELOCITY);
                    g_linearModule[0].SetTargetVelocity(velocity);
                } else if (axis_id == AXIS_Y) {
                    g_linearModule[1].SetMode(x_linear_module::MODULE_MODE_VELOCITY);
                    g_linearModule[1].SetTargetVelocity(velocity);
                }
                set_homing_flag(axis_id, false);
            }
            break;
        
        case CMD_STOP:
            /* [axis_id], AXIS_ALL 支持全停 */
            if (axis_id == AXIS_X || axis_id == AXIS_ALL) {
                stop_axis(g_linearModule[0]);
            }
            if (axis_id == AXIS_Y || axis_id == AXIS_ALL) {
                stop_axis(g_linearModule[1]);
            }
            set_homing_flag(axis_id, false);
            break;
        
        case CMD_QUERY_STATUS:
            /* [axis_id]，统一回复双轴状态 */
            {
                float x_pos = g_linearModule[0].GetPosition();
                float y_pos = g_linearModule[1].GetPosition();
                uint8_t x_status = get_axis_status(g_linearModule[0], s_axis_homing[0]);
                uint8_t y_status = get_axis_status(g_linearModule[1], s_axis_homing[1]);
                float x_vel = step_vel_to_mm_s(g_linearModule[0]);
                float y_vel = step_vel_to_mm_s(g_linearModule[1]);
                uint8_t error_code = (x_status == STATUS_ERROR || y_status == STATUS_ERROR) ? 1U : 0U;

                /* 状态响应有效载荷固定 19 字节 */
                uint8_t response[19] = {0};
                memcpy(&response[0], &x_pos, 4);
                memcpy(&response[4], &y_pos, 4);
                response[8] = x_status;
                response[9] = y_status;
                memcpy(&response[10], &x_vel, 4);
                memcpy(&response[14], &y_vel, 4);
                response[18] = error_code;

                usb_send_response(CMD_STATUS_RESPONSE, response, 19);
            }
            break;
        
        default:
            break;
    }
}
