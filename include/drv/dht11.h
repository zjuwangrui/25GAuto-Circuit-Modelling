#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* 数据引脚：PC6（单总线，兼做输出/输入）
 * 若接到其他引脚，改这两行即可 */
#define DHT11_PORT  GPIOE
#define DHT11_PIN   GPIO_PIN_6

typedef struct {
    uint8_t temp_int;   /* 温度整数部分 °C  (0-50)  */
    uint8_t temp_deci;  /* 温度小数部分（DHT11 恒为 0） */
    uint8_t humi_int;   /* 湿度整数部分 %RH (20-90) */
    uint8_t humi_deci;  /* 湿度小数部分（DHT11 恒为 0） */
} DHT11_Data_t;

/* 返回 HAL_OK = 读取成功且校验通过，HAL_ERROR = 无响应或校验失败 */
void       DHT11_Init(void);
HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *out);

#endif /* __DHT11_H */
