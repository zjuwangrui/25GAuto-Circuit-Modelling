/*
 * panel_ctrl.c —— 串口屏 ↔ signal_out 胶水层 (V5.1 协议)
 *
 * 只处理 3 个控件:
 *   频率输入 (ctrl_id=4,  文本 type=0x11)  → 缓存 last_freq
 *   电压输入 (ctrl_id=7,  文本 type=0x11)  → 缓存 last_vpp
 *   输出按钮 (ctrl_id=11, 按钮 type=0x10)  → signal_out_set(last_freq, last_vpp)
 *
 * ISR 上下文只置 flag + 存参数, 真正动作在 panel_ctrl_task 里做.
 */

#include "module/panel_ctrl.h"
#include "drv/serial_screen.h"
#include "module/signal_out.h"
#include "module/dds.h"
#include "bsp/uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 *  ISR 与 task 之间: 待办 flag + 参数缓存
 * ================================================================== */
static volatile bool     s_pending_output     = false;    /* 输出信号按钮 (走 signal_out_set) */
static volatile bool     s_pending_raw_output = false;    /* 原始输出按钮 (直调 dds_tone_sine, 幅度固定 0.6V) */
static volatile double   s_last_freq_v        = 1000.0;   /* 默认 1kHz */
static volatile float    s_last_vpp_v         = 1.0f;     /* 默认 1V   */

/* ==================================================================
 *  ASCII → 数字 (支持负号 / 小数点)
 * ================================================================== */
static double parse_ascii_double(const uint8_t *s, uint8_t n)
{
    if (!s || n == 0) return 0.0;
    char tmp[16];
    uint8_t m = (n < sizeof(tmp) - 1) ? n : (uint8_t)(sizeof(tmp) - 1);
    memcpy(tmp, s, m);
    tmp[m] = '\0';
    return atof(tmp);
}

/* ==================================================================
 *  drv/serial_screen 帧回调 (中断上下文)
 * ================================================================== */
static void on_frame(uint16_t screen_id, uint16_t ctrl_id, uint8_t ctrl_type,
                     const uint8_t *payload, uint8_t payload_len)
{
    (void)screen_id;
    (void)ctrl_type;

    switch (ctrl_id) {
    case SCREEN_CTRL_FREQ_INPUT:
        /* 文本控件, payload = ASCII 数字字符串 */
        s_last_freq_v = parse_ascii_double(payload, payload_len);
        /* 中断里打印是可以的 (UART_Printf 阻塞发送, 只是慢),
         * 排查完把这行删掉 */
        UART_Printf("[panel][ISR] FREQ input: len=%u ascii=\"%.*s\" parsed=%.4f\r\n",
                    (unsigned)payload_len, (int)payload_len, (const char *)payload,
                    (double)s_last_freq_v);
        break;

    case SCREEN_CTRL_VPP_INPUT:
        s_last_vpp_v = (float)parse_ascii_double(payload, payload_len);
        UART_Printf("[panel][ISR] VPP  input: len=%u ascii=\"%.*s\" parsed=%.4f\r\n",
                    (unsigned)payload_len, (int)payload_len, (const char *)payload,
                    (double)s_last_vpp_v);
        break;

    case SCREEN_CTRL_OUTPUT_BTN:
        /* 按钮 (Ctrl_type=0x10) payload = [Subtype:1B, Status:1B].
         * 只在 Status=0x01 (按下) 时触发, Status=0x00 (松开) 忽略,
         * 避免一次点击触发两次 signal_out_set. */
        if (ctrl_type == 0x10 &&
            payload_len >= 2 &&
            payload[payload_len - 1] == 0x01) {
            s_pending_output = true;
        }
        break;

    case SCREEN_CTRL_RAW_OUTPUT_BTN:
        /* 原始输出按钮: 直接 dds_tone_sine(freq, 0.6V), 不走 signal_out_set,
         * 不做 H(s) 反算. 用来做电路校准/裸测试. */
        if (ctrl_type == 0x10 &&
            payload_len >= 2 &&
            payload[payload_len - 1] == 0x01) {
            s_pending_raw_output = true;
        }
        break;

    default:
        /* 未知控件 ID, 忽略 */
        break;
    }
}

/* ==================================================================
 *  API
 * ================================================================== */

void panel_ctrl_init(void)
{
    screen_init();
    screen_on_frame(on_frame);
}

void panel_ctrl_task(void)
{
    if (s_pending_output) {
        s_pending_output = false;
        double f = s_last_freq_v;
        float  v = s_last_vpp_v;
        UART_Printf("[panel] OUTPUT btn: signal_out_set(%.2f Hz, %.3f V)\r\n",
                    f, (double)v);
        signal_out_set(f, v);
    }

    if (s_pending_raw_output) {
        s_pending_raw_output = false;
        double f = s_last_freq_v;
        /* 多种格式打印, 排查值传递问题 */
        UART_Printf("[panel] RAW OUTPUT btn ==========================\r\n");
        UART_Printf("        s_last_freq_v = %.6f Hz\r\n", (double)s_last_freq_v);
        UART_Printf("        local f       = %.6f Hz  (%e)\r\n", f, f);
        UART_Printf("        as int        = %ld\r\n", (long)f);
        UART_Printf("        about to call: dds_tone_sine(%.2f, 0.6f, 0.0f)\r\n", f);
        dds_tone_sine(f, 0.6f, 0.0f);
        UART_Printf("        dds_tone_sine returned OK\r\n");
    }
}
