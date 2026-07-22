#ifndef __MODULE_PANEL_CTRL_H
#define __MODULE_PANEL_CTRL_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  module/panel_ctrl —— 串口屏与业务模块的胶水层 (V5.1 协议)
 * ===========================================================================
 *
 *  当前只处理 3 个屏控件 (学习/推理按钮以后再加):
 *      控件               类型     ctrl_id
 *      ---------------    ------   -------
 *      频率输入框         文本      4
 *      电压输入框         文本      7
 *      输出信号按钮       按钮      11
 *
 *  流程:
 *      用户屏上输入频率 → 屏发文本上报帧 (ctrl_id=4) → MCU 缓存 last_freq
 *      用户屏上输入电压 → 屏发文本上报帧 (ctrl_id=7) → MCU 缓存 last_vpp
 *      用户点输出信号按钮 → 屏发按钮上报帧 (ctrl_id=11)
 *                                        → MCU 调 signal_out_set(last_freq, last_vpp)
 *
 *  ISR 上下文 (drv/serial_screen 回调) 只置 flag + 缓存参数,
 *  真正动作 (signal_out_set → SPI/HAL) 在 panel_ctrl_task 里做.
 * ===========================================================================
 */

/* --- 屏侧控件 ID (与 VisualTFT 工程一致) --- */
#ifndef SCREEN_CTRL_FREQ_INPUT
#define SCREEN_CTRL_FREQ_INPUT      4       /* 文本控件, type=0x11 */
#endif
#ifndef SCREEN_CTRL_VPP_INPUT
#define SCREEN_CTRL_VPP_INPUT       7       /* 文本控件, type=0x11 */
#endif
#ifndef SCREEN_CTRL_OUTPUT_BTN
#define SCREEN_CTRL_OUTPUT_BTN      11      /* 按钮控件, type=0x10 —— 走 signal_out_set (反算 H(s)) */
#endif
#ifndef SCREEN_CTRL_RAW_OUTPUT_BTN
#define SCREEN_CTRL_RAW_OUTPUT_BTN  40      /* 按钮控件, type=0x10 —— 直接 dds_tone_sine(f, 0.6V) 不反算 */
#endif

/* ==========================================================================
 *  API
 * ========================================================================== */
void panel_ctrl_init(void);      /* 内部会 screen_init + 注册回调 */
void panel_ctrl_task(void);      /* 注册到调度器 (100~200 ms 即可) */

#endif /* __MODULE_PANEL_CTRL_H */
