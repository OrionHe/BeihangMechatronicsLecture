/**
 * 嵌入式端：USB 命令解析器
 * 
 * 这个模块应该在 FreeRTOS USB 解析任务中使用
 * 用来处理来自上位机的通信协议命令
 * 
 * 集成步骤：
 * 1. 将此文件复制到 Firmware_Example/04_XY_Platform_Motion_Control/MDK-ARM/ 目录
 * 2. 在 freertos.c 中包含此头文件
 * 3. 在 USB 解析任务中调用相应处理函数
 */

#ifndef __USB_CMD_PARSER_H
#define __USB_CMD_PARSER_H

#include <stdint.h>
#include <string.h>

/* ===== 协议定义 ===== */

#define FRAME_HEADER 0xAA
#define FRAME_TAIL   0xFF

typedef enum {
    CMD_HOME           = 0x01,    /* 回零 */
    CMD_MOVE_ABS       = 0x02,    /* 绝对位移 */
    CMD_MOVE_REL       = 0x03,    /* 相对位移 */
    CMD_LINE_INTERP    = 0x04,    /* 直线插补 */
    CMD_ARC_INTERP     = 0x05,    /* 圆弧插补 */
    CMD_STOP           = 0x06,    /* 停止 */
    CMD_QUERY_STATUS   = 0x07,    /* 查询状态 */
    CMD_STATUS_RESPONSE = 0xF0    /* 状态响应 */
} UsbCommandType_t;

typedef enum {
    STATUS_IDLE    = 0x00,
    STATUS_HOMING  = 0x01,
    STATUS_MOVING  = 0x02,
    STATUS_ERROR   = 0xFF
} PlatformStatus_t;

/* ===== 函数声明 ===== */

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
    
    /* 外部调用：CDC_Transmit_FS(tx_buf, idx); */
}

/**
 * @brief  处理收到的 USB 命令帧
 * 
 * 使用示例（在 USB 解析任务中）：
 * 
 * void StartUsbParseTask(void *argument)
 * {
 *     uint8_t cmd;
 *     uint8_t *data;
 *     uint8_t data_len;
 *     
 *     for(;;)
 *     {
 *         if (osMessageQueueGet(g_UsbRxQueueHandle, &rxMsg, NULL, 100) == osOK)
 *         {
 *             if (usb_parse_command(rxMsg.data, rxMsg.length, &cmd, &data, &data_len))
 *             {
 *                 usb_handle_command(cmd, data, data_len);
 *             }
 *         }
 *     }
 * }
 */
int usb_parse_command(const uint8_t *frame, uint16_t frame_len,
                      uint8_t *cmd, uint8_t **data, uint8_t *data_len)
{
    if (frame_len < 5) return 0;
    
    if (frame[0] != FRAME_HEADER || frame[frame_len-1] != FRAME_TAIL) {
        return 0;
    }
    
    *cmd = frame[1];
    *data_len = frame[2];
    *data = (uint8_t*)&frame[3];
    
    /* 校验校验和 */
    uint8_t expected_checksum = usb_cmd_calc_checksum(*cmd, *data_len, *data);
    uint8_t actual_checksum = frame[3 + *data_len];
    
    if (expected_checksum != actual_checksum) {
        return 0;
    }
    
    return 1;
}

/**
 * @brief  处理命令的主函数
 * 
 * 需要用户实现以下外部函数：
 * - xy_platform_home()  : 回零
 * - xy_platform_move_abs(x, y, speed) : 绝对位移
 * - xy_platform_move_rel(dx, dy, speed) : 相对位移
 * - xy_platform_line_interp(...) : 直线插补
 * - xy_platform_arc_interp(...) : 圆弧插补
 * - xy_platform_stop() : 停止
 * - xy_platform_get_status(x, y, status, error) : 获取状态
 */
void usb_handle_command(uint8_t cmd, uint8_t *data, uint8_t data_len)
{
    float x, y, dx, dy, speed;
    uint16_t speed_int;
    
    switch (cmd)
    {
        case CMD_HOME:
            /* 回零命令 */
            // xy_platform_home();
            break;
        
        case CMD_MOVE_ABS:
            /* 绝对位移: data = [x(4B) y(4B) speed(2B)] */
            if (data_len >= 10) {
                memcpy(&x, &data[0], 4);
                memcpy(&y, &data[4], 4);
                memcpy(&speed_int, &data[8], 2);
                // xy_platform_move_abs(x, y, speed_int);
            }
            break;
        
        case CMD_MOVE_REL:
            /* 相对位移 */
            if (data_len >= 10) {
                memcpy(&dx, &data[0], 4);
                memcpy(&dy, &data[4], 4);
                memcpy(&speed_int, &data[8], 2);
                // xy_platform_move_rel(dx, dy, speed_int);
            }
            break;
        
        case CMD_LINE_INTERP:
            /* 直线插补: [x1(4B) y1(4B) x2(4B) y2(4B) speed(2B)] */
            if (data_len >= 18) {
                float x1, y1, x2, y2;
                memcpy(&x1, &data[0], 4);
                memcpy(&y1, &data[4], 4);
                memcpy(&x2, &data[8], 4);
                memcpy(&y2, &data[12], 4);
                memcpy(&speed_int, &data[16], 2);
                // xy_platform_line_interp(x1, y1, x2, y2, speed_int);
            }
            break;
        
        case CMD_ARC_INTERP:
            /* 圆弧插补: [xc(4B) yc(4B) radius(4B) angle(4B) speed(2B)] */
            if (data_len >= 18) {
                float xc, yc, radius, angle;
                memcpy(&xc, &data[0], 4);
                memcpy(&yc, &data[4], 4);
                memcpy(&radius, &data[8], 4);
                memcpy(&angle, &data[12], 4);
                memcpy(&speed_int, &data[16], 2);
                // xy_platform_arc_interp(xc, yc, radius, angle, speed_int);
            }
            break;
        
        case CMD_STOP:
            /* 停止 */
            // xy_platform_stop();
            break;
        
        case CMD_QUERY_STATUS:
            /* 查询状态：需要回复 STATUS_RESPONSE */
            {
                float curr_x, curr_y;
                uint8_t status, error;
                
                /* 获取当前状态（用户实现） */
                // xy_platform_get_status(&curr_x, &curr_y, &status, &error);
                
                /* 打包响应 */
                uint8_t response[10];
                memcpy(&response[0], &curr_x, 4);
                memcpy(&response[4], &curr_y, 4);
                response[8] = status;
                response[9] = error;
                
                usb_send_response(CMD_STATUS_RESPONSE, response, 10);
            }
            break;
        
        default:
            break;
    }
}

#endif /* __USB_CMD_PARSER_H */


/* ===== 集成步骤（freertos.c） ===== 

// 1. 在 USER CODE BEGIN Includes 中添加：
#include "usb_cmd_parser.h"
#include "queue.h"

// 2. 定义队列和优先级的宏（USER CODE BEGIN PV）：
#define USB_RX_QUEUE_SIZE 10
typedef struct {
    uint8_t data[64];
    uint32_t length;
} UsbRxMsg_t;

osMessageQueueId_t g_UsbRxQueueHandle = NULL;

// 3. 在 MX_FREERTOS_Init 中创建队列（USER CODE BEGIN RTOS_QUEUES）：
const osMessageQueueAttr_t queue_attr = {
    .name = "UsbRxQueue"
};
g_UsbRxQueueHandle = osMessageQueueNew(USB_RX_QUEUE_SIZE, sizeof(UsbRxMsg_t), &queue_attr);

// 4. 创建解析任务：
osThreadId_t usbParseTaskHandle;
const osThreadAttr_t usbParseTask_attributes = {
    .name = "usbParseTask",
    .stack_size = 256 * 4,
    .priority = (osPriority_t) osPriorityNormal,
};

usbParseTaskHandle = osThreadNew(StartUsbParseTask, NULL, &usbParseTask_attributes);

// 5. 实现任务函数：
void StartUsbParseTask(void *argument)
{
    UsbRxMsg_t rxMsg;
    uint8_t cmd;
    uint8_t *data;
    uint8_t data_len;
    
    for(;;)
    {
        if (osMessageQueueGet(g_UsbRxQueueHandle, &rxMsg, NULL, 100) == osOK)
        {
            if (usb_parse_command(rxMsg.data, rxMsg.length, &cmd, &data, &data_len))
            {
                usb_handle_command(cmd, data, data_len);
            }
        }
    }
}

// 6. 在 usbd_cdc_if.c 的 CDC_Receive_FS 中将数据压入队列：
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
    extern osMessageQueueId_t g_UsbRxQueueHandle;
    extern typedef struct { uint8_t data[64]; uint32_t length; } UsbRxMsg_t;
    
    if (g_UsbRxQueueHandle != NULL && Len && *Len > 0)
    {
        UsbRxMsg_t msg;
        msg.length = *Len > 64 ? 64 : *Len;
        memcpy(msg.data, Buf, msg.length);
        osMessageQueuePut(g_UsbRxQueueHandle, &msg, 0, 0);
    }
    
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (USBD_OK);
}

*/
