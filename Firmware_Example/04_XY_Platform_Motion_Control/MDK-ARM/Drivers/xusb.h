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

extern uint8_t flag_usb;
extern uint32_t Len_usb;  
extern uint8_t Buffer_usb[512];
/* ===== 函数声明 ===== */
void usb_send_response(uint8_t cmd, const uint8_t *data, uint8_t data_len);

int usb_parse_command(const uint8_t *frame, uint16_t frame_len,
                      uint8_t *cmd, uint8_t **data, uint8_t *data_len);

void usb_handle_command(uint8_t cmd, uint8_t *data, uint8_t data_len);


#endif