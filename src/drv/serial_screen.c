/*
 * serial_screen.c —— 广州大彩串口屏驱动 (V5.1 + 自定义按钮帧)
 *
 * RX 状态机: 从 EE 开始收字节到缓冲, 每收一字节就检查帧是否完整.
 *   - type = 0x10 (按钮): 固定 10 字节, **无帧尾** (VisualTFT 里自定义
 *                          按钮指令不能含 0xFF, 所以不用 FF FC FF FF)
 *   - type = 0x11 (文本): 变长 payload + 0x00 终止 + FF FC FF FF 帧尾
 *                          (大彩标准, 屏自动生成)
 *
 * 中断上下文: 尽量短平快, 只解析 + 调回调 (回调里应该只置 flag).
 */

#include "drv/serial_screen.h"
#include "bsp/uart.h"
#include <string.h>

/* ---------- 常量 ---------- */
#define FRAME_MAX          40                              /* 一帧最大字节 */
static const uint8_t FRAME_END[4] = { 0xFF, 0xFC, 0xFF, 0xFF };

/* CMD */
#define CMD_UP_CTRL        0x11    /* 屏 → MCU 组态控件事件 */
#define CMD_DOWN_TEXT      0x10    /* MCU → 屏 更新文本控件 */

/* 控件类型 */
#define CTYPE_BUTTON       0x10    /* 按钮控件 */
#define CTYPE_TEXT         0x11    /* 文本控件 */

/* 按钮固定帧长: EE(1) + B1(1) + 11(1) + page(2) + ctrl(2) + type(1) + payload(2) = 10 */
#define BUTTON_FRAME_LEN   10
/* 文本最小帧长: EE B1 11 + 4 addr + 1 type + 至少 1 字节数据 + 00 终止 + 4 footer = 13 */
#define TEXT_FRAME_MIN     13

/* ---------- 状态 ---------- */
static screen_frame_cb_t s_frame_cb   = NULL;
static uint8_t           s_buf[FRAME_MAX];
static uint8_t           s_len         = 0;
static bool              s_collecting  = false;

/* ==================================================================
 *  帧完整时调用: 解析并转给上层
 * ================================================================== */
static void dispatch_frame(const uint8_t *f, uint8_t n)
{
    if (n < BUTTON_FRAME_LEN) return;
    if (f[0] != 0xEE || f[1] != 0xB1 || f[2] != CMD_UP_CTRL) return;

    uint16_t screen_id = ((uint16_t)f[3] << 8) | f[4];
    uint16_t ctrl_id   = ((uint16_t)f[5] << 8) | f[6];
    uint8_t  ctrl_type = f[7];

    const uint8_t *payload;
    uint8_t        payload_len;

    if (ctrl_type == CTYPE_BUTTON) {
        /* 按钮: 固定长度, 无 footer */
        payload     = &f[8];
        payload_len = (uint8_t)(n - 8);         /* 一般 = 2 (Subtype + Status) */
    } else {
        /* 文本或其他: 有 FF FC FF FF footer, payload 末尾可能有 0x00 终止 */
        if (n < TEXT_FRAME_MIN) return;
        payload     = &f[8];
        payload_len = (uint8_t)(n - 8 - 4);     /* 减去 header(8) + footer(4) */
        /* 文本类剥掉末尾 0x00 终止符 */
        if (ctrl_type == CTYPE_TEXT && payload_len > 0 &&
            payload[payload_len - 1] == 0x00) {
            payload_len--;
        }
    }

    if (s_frame_cb) {
        s_frame_cb(screen_id, ctrl_id, ctrl_type, payload, payload_len);
    }
}

/* ==================================================================
 *  bsp/uart 每收到一个 UART2 字节调用此函数 (中断上下文)
 * ================================================================== */
void SCREEN_UART_IRQHandler(uint8_t byte)
{
    if (!s_collecting) {
        /* 等待帧头 EE */
        if (byte == 0xEE) {
            s_buf[0]     = 0xEE;
            s_len        = 1;
            s_collecting = true;
        }
        return;
    }

    /* 缓冲溢出保护 */
    if (s_len >= FRAME_MAX) {
        s_collecting = false;
        s_len = 0;
        return;
    }

    s_buf[s_len++] = byte;

    /* --- 情况 A: 按钮帧 (固定 10 字节, 无 footer) ---
     * 收够 10 字节时, 检查 header + type=0x10:
     *   如果匹配, 立即完成 (不等 footer, 屏也不会发 footer);
     *   如果 type != 0x10 (文本或未知), 继续收等 footer. */
    if (s_len == BUTTON_FRAME_LEN &&
        s_buf[1] == 0xB1 && s_buf[2] == CMD_UP_CTRL &&
        s_buf[7] == CTYPE_BUTTON) {
        dispatch_frame(s_buf, s_len);
        s_collecting = false;
        s_len = 0;
        return;
    }

    /* --- 情况 B: 文本帧 (变长, 以 FF FC FF FF 结尾) --- */
    if (s_len >= TEXT_FRAME_MIN &&
        memcmp(&s_buf[s_len - 4], FRAME_END, 4) == 0) {
        dispatch_frame(s_buf, s_len);
        s_collecting = false;
        s_len = 0;
    }
}

/* ==================================================================
 *  API
 * ================================================================== */

void screen_init(void)
{
    s_frame_cb   = NULL;
    s_len        = 0;
    s_collecting = false;
    MX_USART2_UART_Init();
}

void screen_on_frame(screen_frame_cb_t cb)
{
    s_frame_cb = cb;
}

void screen_send_raw(const uint8_t *data, uint16_t len)
{
    UART2_SendBytes(data, len);
}

/* MCU → 屏 文本控件更新:
 *   EE B1 10 [screen_id=0] [ctrl_id] [ASCII] FF FC FF FF */
void screen_set_text(uint16_t ctrl_id, const char *ascii)
{
    uint8_t  buf[64];
    uint16_t i = 0;

    buf[i++] = 0xEE;
    buf[i++] = 0xB1;
    buf[i++] = CMD_DOWN_TEXT;
    buf[i++] = 0x00;                              /* Screen_id hi */
    buf[i++] = 0x00;                              /* Screen_id lo */
    buf[i++] = (uint8_t)(ctrl_id >> 8);           /* Ctrl_id hi */
    buf[i++] = (uint8_t)(ctrl_id & 0xFF);         /* Ctrl_id lo */

    if (ascii) {
        while (*ascii && i < (uint16_t)(sizeof(buf) - 4)) {
            buf[i++] = (uint8_t)(*ascii++);
        }
    }

    buf[i++] = 0xFF;
    buf[i++] = 0xFC;
    buf[i++] = 0xFF;
    buf[i++] = 0xFF;

    UART2_SendBytes(buf, i);
}
