#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ===== ADC1 高速采样服务 =====
 *
 * ADC1 通过 TIM3 TRGO 触发，DMA1_Ch1 循环搬运。
 * 单通道，采样率可配（默认 40kHz），双缓冲：半满/全满
 * 中断分别调用 fft_feed（或用户注册的回调）。
 *
 * 需要慢速传感器（电压/温度/光敏等）另开 ADC2 或走 GPIO/I2C。
 * 框架故意不再提供 ADC1 轮询接口 —— 避免与 DMA 模式互相打架。
 */

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim3;
extern DMA_HandleTypeDef hdma_adc1;

/* 采样一段完成时的回调：由 ADC 模块在中断里调用。
 * raw 指向新到的一段 uint16_t 数据（长度 n）。*/
typedef void (*adc_fft_cb_t)(const uint16_t *raw, uint16_t n);

/* ---- 初始化 ----
 * 只做时钟使能 + ADC 周边配置。真正的转换在 ADC_StartFFT 里启动。*/
void MX_ADC1_Init(void);

/* ---- FFT 高速采样 ---- */
void ADC_SetFFTCallback(adc_fft_cb_t cb);
bool ADC_StartFFT(uint32_t channel, uint32_t fs_hz,
                  uint16_t *buf, uint16_t buf_len);   /* buf_len 必须偶数 */
void ADC_StopFFT(void);
bool ADC_IsFFTRunning(void);

#endif /* __BSP_ADC_H */
