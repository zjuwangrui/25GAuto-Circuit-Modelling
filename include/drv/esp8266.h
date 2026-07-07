#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f1xx_hal.h"
#include "bsp/uart.h"
#include <stdbool.h>

/* ===== 用户配置（按实际环境修改） ===== */
#define AP_SSID           "VTL_Sensor"
#define AP_PASS           "12345678"
#define PHONE_IP          "192.168.1.2"
#define PHONE_PORT        8080

#define ESP_TIMEOUT_SHORT 3000U   /* ms */
#define ESP_TIMEOUT_LONG  8000U   /* ms */

/* ===== 野火指南者板 ESP8266 硬件引脚 ===== */
/* USART3 作为通信口（PB10 TX / PB11 RX） */
#define ESP_UART          huart3

/* CH_PD（使能脚）：PB8 高电平有效，不拉高模块不工作 */
#define ESP_CH_PD_PORT    GPIOB
#define ESP_CH_PD_PIN     GPIO_PIN_8
/* RST（复位脚）：PB9 低电平复位 */
#define ESP_RST_PORT      GPIOB
#define ESP_RST_PIN       GPIO_PIN_9

#define ESP_CH_ENABLE()   HAL_GPIO_WritePin(ESP_CH_PD_PORT, ESP_CH_PD_PIN, GPIO_PIN_SET)
#define ESP_CH_DISABLE()  HAL_GPIO_WritePin(ESP_CH_PD_PORT, ESP_CH_PD_PIN, GPIO_PIN_RESET)
#define ESP_RST_HIGH()    HAL_GPIO_WritePin(ESP_RST_PORT,   ESP_RST_PIN,   GPIO_PIN_SET)
#define ESP_RST_LOW()     HAL_GPIO_WritePin(ESP_RST_PORT,   ESP_RST_PIN,   GPIO_PIN_RESET)

/* ===== 返回码 ===== */
typedef enum {
    ESP_OK          = 0,
    ESP_ERR_AT,
    ESP_ERR_TIMEOUT,
    ESP_ERR_PARAM,
} ESP_Result;

/* ===== 统计信息 ===== */
typedef struct {
    bool     ap_ready;
    uint32_t send_count;
    uint32_t fail_count;
    bool     last_send_ok;
} ESP_Stats;

extern ESP_Stats g_esp_stats;

/* ===== 公开接口 ===== */
ESP_Result ESP_Init(void);
/* voltage: 电压V, led_on: LED状态, light: 光照原始值0-4095, temp: 温度°C
 * out_thr: 返回电压阈值, out_light_thr: 返回光照阈值, out_temp_thr: 返回温度阈值 */
ESP_Result ESP_PostSensor(float voltage, bool led_on, uint32_t light,
                          uint8_t temp,
                          float *out_thr, uint32_t *out_light_thr,
                          uint8_t *out_temp_thr);
void       ESP_UART_IRQHandler(void);

#endif /* __ESP8266_H */
