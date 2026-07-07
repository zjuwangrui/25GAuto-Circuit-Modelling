#include "bsp/led.h"

/* 硬件表：换板时只改这一处 */
typedef struct { GPIO_TypeDef *port; uint16_t pin; bool active_low; } led_pin_t;

static const led_pin_t s_led[LED_COUNT] = {
    [LED_RED  ] = { GPIOB, GPIO_PIN_5, true },
    [LED_GREEN] = { GPIOB, GPIO_PIN_0, true },
};

void LED_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    for (int i = 0; i < LED_COUNT; ++i) {
        g.Pin = s_led[i].pin;
        HAL_GPIO_Init(s_led[i].port, &g);
        LED_Off((led_id_t)i);
    }
}

void LED_Set(led_id_t id, bool on)
{
    if ((unsigned)id >= LED_COUNT) return;
    const led_pin_t *p = &s_led[id];
    GPIO_PinState s = (on ^ p->active_low) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(p->port, p->pin, s);
}

void LED_On    (led_id_t id) { LED_Set(id, true);  }
void LED_Off   (led_id_t id) { LED_Set(id, false); }
void LED_Toggle(led_id_t id) {
    if ((unsigned)id >= LED_COUNT) return;
    HAL_GPIO_TogglePin(s_led[id].port, s_led[id].pin);
}
