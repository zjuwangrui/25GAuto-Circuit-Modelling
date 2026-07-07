#include "bsp/lcd.h"
#include "bsp/fonts.h"
#include <stdarg.h>
#include <stdio.h>

void LCD_Init(void)
{
    ILI9341_Init();
    ILI9341_GramScan(6);                   /* 横屏，320×240 */
    ILI9341_BackLed_Control(ENABLE);
    LCD_SetFont(&Font8x16);
    LCD_Clear(BLACK);
}

void LCD_Clear(uint16_t bg)
{
    LCD_SetColors(WHITE, bg);
    ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
}

void LCD_DrawText(uint16_t x, uint16_t y, const char *s)
{
    ILI9341_DispString_EN(x, y, (char *)s);
}

void LCD_DrawTextC(uint16_t x, uint16_t y, uint16_t fg, uint16_t bg, const char *s)
{
    LCD_SetColors(fg, bg);
    ILI9341_DispString_EN(x, y, (char *)s);
}

void LCD_DrawTextf(uint16_t x, uint16_t y, uint16_t fg, uint16_t bg,
                   const char *fmt, ...)
{
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LCD_SetColors(fg, bg);
    ILI9341_DispString_EN(x, y, buf);
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    LCD_SetColors(color, color);
    ILI9341_DrawRectangle(x, y, w, h, 1);
}
