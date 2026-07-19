/*
 * serial_screen.c —— 广州大彩串口屏 驱动
 *
 * 协议解析在中断里做 (SCREEN_UART_IRQHandler 被 bsp/uart 里的 USART2_IRQHandler
 * 每字节调一次). 两个状态机并行跑, 分别识别短触摸帧和长参数帧.
 */

#include "drv/serial_screen.h"
#include "bsp/uart.h"
#include <string.h>
#include <stdio.h>

/* -------- 命令帧头/尾 (与例程一致) -------- */
static const uint8_t CMD_HEAD[6] = { 0xEE, 0xB1, 0x10, 0x00, 0x00, 0x00 };
static const uint8_t CMD_END [4] = { 0xFF, 0xFC, 0xFF, 0xFF };

/* -------- 回调指针 -------- */
static screen_touch_cb_t s_touch_cb = NULL;
static screen_input_cb_t s_input_cb = NULL;

/* -------- 触摸帧解析状态机 (0xFE d0 d1 0xFF) -------- */
static uint8_t s_touch_state = 0;   /* 0 = 等 0xFE, 1 = 收 2 字节, 2 = 等 0xFF */
static uint8_t s_touch_buf[2];
static uint8_t s_touch_idx = 0;

/* -------- 输入帧解析状态机 (0x04 0x11 payload... 0x00 0xFF) -------- */
#define SCREEN_INPUT_MAX 32
static uint8_t s_input_state = 0;   /* 0 = 等 0x04, 1 = 等 0x11, 2 = 收 payload, 3 = 等 0xFF */
static uint8_t s_input_buf[SCREEN_INPUT_MAX];
static uint8_t s_input_len = 0;

/* ==================================================================
 *  生命周期
 * ================================================================== */

void screen_init(void)
{
    MX_USART2_UART_Init();
    s_touch_state = 0;
    s_touch_idx   = 0;
    s_input_state = 0;
    s_input_len   = 0;
}

void screen_on_touch(screen_touch_cb_t cb) { s_touch_cb = cb; }
void screen_on_input(screen_input_cb_t cb) { s_input_cb = cb; }

/* ==================================================================
 *  发送
 * ================================================================== */

void screen_send_raw(const uint8_t *data, uint16_t len)
{
    UART2_SendBytes(data, len);
}

void screen_send_text_cmd(uint8_t id, const uint8_t *data, uint8_t len)
{
    UART2_SendBytes(CMD_HEAD, 6);
    UART2_SendByte(id);
    if (data && len) UART2_SendBytes(data, len);
    UART2_SendBytes(CMD_END, 4);
}

void screen_set_text(uint8_t id, const char *s)
{
    if (!s) { screen_send_text_cmd(id, NULL, 0); return; }
    uint16_t n = (uint16_t)strlen(s);
    screen_send_text_cmd(id, (const uint8_t *)s, (uint8_t)(n > 255 ? 255 : n));
}

/* 把 uint32 转成不带前导零的十进制 ASCII (最大 10 位).
 * 返回字节数. 缓冲区 buf 至少 10 字节. */
static uint8_t u32_to_ascii(uint32_t v, uint8_t *buf)
{
    if (v == 0) { buf[0] = '0'; return 1; }
    char tmp[11];
    uint8_t n = 0;
    while (v > 0) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    /* 反转 */
    for (uint8_t i = 0; i < n; ++i) buf[i] = (uint8_t)tmp[n - 1 - i];
    return n;
}

void screen_set_freq_text(uint8_t id, uint32_t freq_hz)
{
    uint8_t buf[10];
    uint8_t n = u32_to_ascii(freq_hz, buf);
    screen_send_text_cmd(id, buf, n);
}

void screen_set_vpp_text(uint8_t id, float v)
{
    /* 格式 "%.2f", 允许负号. snprintf 输出到栈缓冲, 再当 payload 发. */
    char tmp[16];
    int n = snprintf(tmp, sizeof(tmp), "%.2f", (double)v);
    if (n < 0) n = 0;
    if (n > (int)sizeof(tmp)) n = (int)sizeof(tmp);
    screen_send_text_cmd(id, (const uint8_t *)tmp, (uint8_t)n);
}

/* ==================================================================
 *  RX: 由 bsp/uart 里的 USART2_IRQHandler 每字节调用一次
 * ==================================================================
 *  两个状态机并行处理.
 *   [A] 短触摸: 0xFE [d0] [d1] 0xFF
 *   [B] 长输入: 0x04 0x11 [payload...] 0x00 0xFF
 * ================================================================== */
void SCREEN_UART_IRQHandler(uint8_t byte)
{
    /* ---- 状态机 A: 触摸事件 ---- */
    switch (s_touch_state) {
    case 0:
        if (byte == 0xFE) { s_touch_state = 1; s_touch_idx = 0; }
        break;
    case 1:
        s_touch_buf[s_touch_idx++] = byte;
        if (s_touch_idx >= 2) s_touch_state = 2;
        break;
    case 2:
        if (byte == 0xFF) {
            if (s_touch_cb) s_touch_cb(s_touch_buf[0], s_touch_buf[1]);
        }
        s_touch_state = 0;
        break;
    default:
        s_touch_state = 0;
        break;
    }

    /* ---- 状态机 B: 参数输入 ---- */
    switch (s_input_state) {
    case 0:
        if (byte == 0x04) s_input_state = 1;
        break;
    case 1:
        if (byte == 0x11) { s_input_state = 2; s_input_len = 0; }
        else              s_input_state = 0;
        break;
    case 2:
        if (byte == 0x00) {
            s_input_state = 3;   /* 等 0xFF 收尾 */
        } else if (s_input_len < SCREEN_INPUT_MAX) {
            s_input_buf[s_input_len++] = byte;
        } else {
            /* payload 过长, 丢弃这一帧 */
            s_input_state = 0;
        }
        break;
    case 3:
        if (byte == 0xFF) {
            if (s_input_cb) s_input_cb(s_input_buf, s_input_len);
        }
        s_input_state = 0;
        break;
    default:
        s_input_state = 0;
        break;
    }
}
