#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp/bsp_ili9341_lcd.h"

/*
 * LCD 通用封装。底层芯片驱动在 bsp_ili9341_lcd。
 * 只暴露"上电初始化 + 常用绘制原语"，不假设应用页面结构。
 * 应用页面（首页/频谱页/参数页...）请在 module/ui 或用户自己的
 * module/xxx 里写。
 */

void LCD_Init(void);                                 /* 上电初始化 + 清屏 + 背光 */
void LCD_Clear(uint16_t bg);

/* 文字：fg/bg 颜色需先自己设置好，或直接用便捷版本 */
void LCD_DrawText  (uint16_t x, uint16_t y, const char *s);
void LCD_DrawTextC (uint16_t x, uint16_t y, uint16_t fg, uint16_t bg,
                    const char *s);
void LCD_DrawTextf (uint16_t x, uint16_t y, uint16_t fg, uint16_t bg,
                    const char *fmt, ...);

/* 矩形填充（用于状态灯、条形谱等） */
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                  uint16_t color);

#endif /* __BSP_LCD_H */
