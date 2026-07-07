#ifndef __FONT_H
#define __FONT_H

#include "stm32f1xx_hal.h"

typedef struct _tFont {
    const uint8_t *table;
    uint16_t Width;
    uint16_t Height;
} sFONT;

extern sFONT Font24x32;
extern sFONT Font16x24;
extern sFONT Font8x16;

/* LCD_GetFont declared here to allow LINE()/LINEY() macros;
   implemented in bsp_ili9341_lcd.c */
sFONT *LCD_GetFont(void);

#define LINE(x)  ((x) * (((sFONT *)LCD_GetFont())->Height))
#define LINEY(x) ((x) * (((sFONT *)LCD_GetFont())->Width))

#define GetGBKCode(ucBuffer, usChar)

#endif /* __FONT_H */
