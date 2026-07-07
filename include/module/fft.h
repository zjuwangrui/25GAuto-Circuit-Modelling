#ifndef __MODULE_FFT_H
#define __MODULE_FFT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * FFT + THD 模块（Q15 定点，基于 CMSIS-DSP）
 *
 * 数据流：
 *   ADC1 (TIM3 触发) --DMA循环-->  raw[2N] 半/满中断 --> fft_feed()
 *                                                        |
 *                             去直流 + Hanning 窗         v
 *                             arm_rfft_q15                spec
 *                             arm_cmplx_mag_q15           mag
 *                             找基频 + 算 THD             fft_result_t
 *
 * 用法：
 *   fft_init();                     // 已在 main.c 里调
 *   在 terminal 里敲：
 *       fft.start                  // 打开 ADC DMA + 使能 fft 任务
 *       fft.stop
 *       fft.info                   // 打印基频 / THD / 帧号
 *
 * 默认参数：单通道 IN1 (PA1)，Fs=40kHz，N=1024，分辨率 ~39Hz
 * 可用 fft_configure() 覆盖。
 */

#define FFT_N_POINTS        2048U
#define FFT_DEFAULT_FS_HZ   1000000U  /* 采样率 1MHz*/
#define FFT_DEFAULT_CHANNEL 1U          /* ADC_CHANNEL_1 = PA1 */

typedef struct {
    float    f0_hz;                        /* 基频 Hz            */
    float    thd_percent;                  /* 失真度 %            */
    float    mag_db[FFT_N_POINTS/2];       /* 幅度谱 dB，供 UI    */
    uint32_t frame_id;                     /* 每算一帧自增        */
    uint32_t fs_hz;                        /* 当前采样率          */
} fft_result_t;

void  fft_init(void);
void  fft_task(void);
bool  fft_start(void);
void  fft_stop (void);
bool  fft_is_running(void);

/* 采样前调用。channel 是 ADC_CHANNEL_x，fs_hz 采样率。 */
void  fft_configure(uint32_t channel, uint32_t fs_hz);

const fft_result_t *fft_get_result(void);

#endif /* __MODULE_FFT_H */
