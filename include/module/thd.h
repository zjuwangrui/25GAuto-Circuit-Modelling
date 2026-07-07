#ifndef __MODULE_THD_H
#define __MODULE_THD_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  THD 测量模块（基于 Goertzel）
 * ===========================================================================
 *
 * 输入通路：
 *   ADC1 (TIM3 触发 40kHz) → DMA1 双缓冲 → thd_feed()（ISR 上下文）
 *
 * 处理流程（thd_task 里）：
 *   1) 两级 Goertzel 扫描找出基频 f0
 *      - 粗扫：50 Hz 起，50 Hz 步长，扫到 4 kHz（保证 5 次谐波仍在 Nyquist 内）
 *      - 细扫：粗扫峰值 ±50 Hz 内，5 Hz 步长
 *      - 抛物线插值：3 点拟合，精度 ~0.5 Hz
 *   2) 用 f0 算 1..5 次谐波幅度 H[0]..H[4]
 *   3) THD = sqrt(H2²+H3²+H4²+H5²) / H1 × 100
 *
 * 用法（与 fft 模块并存但互斥使用 ADC1）：
 *   thd_start();          // 独占 ADC，开始采样
 *   ...
 *   const thd_result_t *r = thd_get_result();
 *   ...
 *   thd_stop();
 *
 * Terminal:
 *   thd.start / thd.stop / thd.info / thd.config <ch> <fs>
 * ===========================================================================
 */

#define THD_N_POINTS         1024U
#define THD_DEFAULT_FS_HZ    40000U
#define THD_DEFAULT_CHANNEL  1U       /* ADC_CHANNEL_1 = PA1 */
#define THD_MAX_HARMONIC     5        /* 算到 5 次谐波 */

typedef struct {
    float    f0_hz;                                /* 检测到的基频 Hz            */
    float    harmonic[THD_MAX_HARMONIC];           /* H[0]=H1 幅度 … H[4]=H5     */
    float    harmonic_phase[THD_MAX_HARMONIC];     /* 相位 (radians, -π..π)     */
    float    thd_percent;                          /* 失真度 %                    */
    uint32_t frame_id;                             /* 每算一帧自增                */
    uint32_t fs_hz;                                /* 当前采样率                  */
} thd_result_t;

void  thd_init(void);
void  thd_task(void);                      /* 注册到调度器 */
bool  thd_start(void);
void  thd_stop (void);
bool  thd_is_running(void);
void  thd_configure(uint32_t channel, uint32_t fs_hz);
const thd_result_t *thd_get_result(void);

#endif /* __MODULE_THD_H */
