#ifndef __BSP_SPI_H
#define __BSP_SPI_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* ===== SPI1 (PA5=SCK, PA6=MISO, PA7=MOSI)  Mode 0，MSB first =====
 *
 * 片选 CS 由使用者自己管（不同外设各有自己的 CS），本模块不管 CS。
 * 主频 = APB2 / prescaler = 72MHz / 8 = 9MHz。稳妥、够快。
 *
 * 若某个外设需要不同的 SPI 模式（比如 Mode 3）或不同波特率，用
 *   SPI_SetMode3Fast() / SPI_SetMode0Fast() 等辅助函数重配，不动 GPIO。
 */

extern SPI_HandleTypeDef hspi1;

void  MX_SPI1_Init(void);

/* 同步收发单字节：发 tx，同时读回 miso */
uint8_t SPI1_Xfer(uint8_t tx);
/* 只写：发一串字节，抛弃 MISO 数据 */
void    SPI1_Write(const uint8_t *data, uint16_t len);
/* 全双工收发：tx/rx 缓冲区可以指同一处，就地覆盖 */
void    SPI1_Xchg (const uint8_t *tx, uint8_t *rx, uint16_t len);

#endif /* __BSP_SPI_H */
