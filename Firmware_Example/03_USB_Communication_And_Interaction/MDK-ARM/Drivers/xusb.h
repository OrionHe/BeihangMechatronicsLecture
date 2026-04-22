#ifndef __USB_CMD_PARSER_H
#define __USB_CMD_PARSER_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 协议定义 ===== */

#define FRAME_HEADER 0xAA
#define FRAME_TAIL   0xFF

typedef enum {
    AXIS_X   = 0x00,
    AXIS_Y   = 0x01,
    AXIS_ALL = 0xFF
} AxisId_t;

typedef enum {
    CMD_HOME            = 0x01,   /* [axis_id] */
    CMD_MOVE_ABS        = 0x02,   /* [axis_id][position(float,4B)][speed(uint16,2B)] */
    CMD_SET_VELOCITY    = 0x03,   /* [axis_id][velocity(uint16,2B)] */
    CMD_STOP            = 0x06,   /* [axis_id] */
    CMD_QUERY_STATUS    = 0x07,   /* [axis_id] */
    CMD_STATUS_RESPONSE = 0xF0    /* [x_pos(4B)][y_pos(4B)][x_status(1B)][y_status(1B)][x_vel(2B)][y_vel(2B)][error(1B)] */
} UsbCommandType_t;

typedef enum {
    STATUS_IDLE    = 0x00,
    STATUS_HOMING  = 0x01,
    STATUS_MOVING  = 0x02,
    STATUS_ERROR   = 0xFF
} PlatformStatus_t;

#define USB_RX_THREAD_FLAG_DATA (1UL << 0)

/* ===== 函数声明 ===== */
void usb_send_response(uint8_t cmd, const uint8_t *data, uint8_t data_len);

int usb_parse_command(const uint8_t *frame, uint16_t frame_len,
                      uint8_t *cmd, uint8_t **data, uint8_t *data_len);

void usb_handle_command(uint8_t cmd, uint8_t *data, uint8_t data_len);

#ifdef __cplusplus
}
#endif

#endif
