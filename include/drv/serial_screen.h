#ifndef __DRV_SERIAL_SCREEN_H
#define __DRV_SERIAL_SCREEN_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  广州大彩串口屏 (GZ-DC) 通信驱动
 *  参考手册: https://www.gz-dc.com/ ... DC80480GM070_1X(TCN)DATASHEET
 *  参考例程: resources/（全国一等奖）ZJA506.../Hardware/Serial_Screen.c
 * ===========================================================================
 *
 *  串口配置:
 *    UART2  PA2(TX) / PA3(RX)  115200 8N1
 *
 *  MCU → 屏  命令帧 (文本类):
 *    0xEE 0xB1 0x10 0x00 0x00 0x00  [id]  [data bytes...]  0xFF 0xFC 0xFF 0xFF
 *                                         ^^^^^^^^^^^^^^^ 一般是 ASCII 数字/汉字编码
 *
 *  屏 → MCU  上报帧 (两种):
 *    [A] 短触摸事件:   0xFE  [d0]  [d1]  0xFF                 (4 字节)
 *                          d0 一般是控件 ID, d1 是事件码 (按下/释放...).
 *    [B] 长参数输入:   0x04  0x11  [ctrl_id]  [ASCII digits...]  0x00  0xFF
 *                          payload = [ctrl_id, ASCII digits] (回调里剥掉 04/11/00/FF)
 *                          payload[0] = 控件 ID, payload[1..] = ASCII 数字字符
 *
 *  典型用法:
 *    screen_init();
 *    screen_set_freq_text(0x10, 12345);       // 把控件 id=0x10 的文本设为 "12345"
 *    screen_set_vpp_text (0x11, 2.5f);        // "2.5"
 *    screen_on_touch(my_touch_handler);       // 注册短触摸事件回调
 *    screen_on_input(my_input_handler);       // 注册长参数输入回调
 *
 *  分层:
 *    drv/serial_screen  ← 你在这, 只做协议帧 (打包/解包)
 *    module/xxx         ← 上层业务: 把触摸事件映射成 signal_out_set 等
 * ===========================================================================
 */

/* -------- 回调类型 -------- */

/* 短触摸事件: 屏发来 4 字节 0xFE [d0] [d1] 0xFF 后触发. */
typedef void (*screen_touch_cb_t)(uint8_t d0, uint8_t d1);

/* 长参数输入: 屏发来 0x04 0x11 [payload...] 0x00 0xFF 后触发.
 * payload_len = 收到的中间字节数 (不含 0x04 0x11 头和 0x00 0xFF 尾). */
typedef void (*screen_input_cb_t)(const uint8_t *payload, uint8_t payload_len);

/* -------- 生命周期 -------- */

void screen_init(void);                       /* 内部调用 MX_USART2_UART_Init */

/* 注册回调 (在中断里被调, 尽量短平快, 别 HAL_Delay) */
void screen_on_touch(screen_touch_cb_t cb);
void screen_on_input(screen_input_cb_t cb);

/* -------- 低层发送 -------- */

/* 发送任意字节 (会用 USART2 阻塞方式发出) */
void screen_send_raw(const uint8_t *data, uint16_t len);

/* 发送标准文本命令:
 *   0xEE 0xB1 0x10 0x00 0x00 0x00 [id] [data...] 0xFF 0xFC 0xFF 0xFF */
void screen_send_text_cmd(uint8_t id, const uint8_t *data, uint8_t len);

/* -------- 高层便捷 API -------- */

/* 把控件 id 的文本设为整型频率 (Hz), 屏上显示为不定位数十进制数字. */
void screen_set_freq_text(uint8_t id, uint32_t freq_hz);

/* 把控件 id 的文本设为电压值, 显示格式 X.XX (两位小数).
 *   例: screen_set_vpp_text(id, 2.34f)  →  屏上显示 "2.34" */
void screen_set_vpp_text(uint8_t id, float v);

/* 通用 ASCII 字符串填写. len 不含结束符. */
void screen_set_text(uint8_t id, const char *s);

#endif /* __DRV_SERIAL_SCREEN_H */
