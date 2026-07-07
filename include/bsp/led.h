#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>

/*
 * 板载 LED（野火指南者：PB5=红，PB0=绿，均低电平点亮）。
 * 换板时改这几行即可，主程序不需要动。
 */

typedef enum {
    LED_RED   = 0,
    LED_GREEN = 1,
    LED_COUNT
} led_id_t;

void LED_Init(void);
void LED_On   (led_id_t id);
void LED_Off  (led_id_t id);
void LED_Set  (led_id_t id, bool on);
void LED_Toggle(led_id_t id);

#endif /* __BSP_LED_H */
