/*
 * fft.c —— FFT + THD 实现（Q15 定点，CMSIS-DSP）
 *
 * 前置条件（platformio.ini）：
 *   build_flags += -DARM_MATH_CM3
 *   lib_deps    += CMSIS-DSP
 *
 * 若 CMSIS-DSP 尚未拉下来，注释掉下面的 USE_CMSIS_DSP，
 * fft 任务会正常运行但结果不计算（只累加 frame_id），
 * 便于先把框架跑通再逐步启用 DSP。
 */

#define USE_CMSIS_DSP  1

#include "module/fft.h"
#include "core/scheduler.h"
#include "bsp/adc.h"
#include <string.h>
#include <math.h>

#if USE_CMSIS_DSP
  #include "arm_math.h"
  static arm_rfft_instance_q15 s_rfft;
#endif

/* ===== 内部状态 ===== */
static fft_result_t s_result;
static uint32_t     s_channel = FFT_DEFAULT_CHANNEL;
static uint32_t     s_fs_hz   = FFT_DEFAULT_FS_HZ;

/* DMA 目标缓冲：双帧（半满/全满各一帧） */
static uint16_t s_dma_buf[FFT_N_POINTS * 2];

/* 待处理帧：从 ISR 拷入，标记 ready 后 fft_task 处理 */
static volatile int16_t s_frame[FFT_N_POINTS];
static volatile bool    s_frame_ready = false;

/* Hanning 窗（Q15，运行时生成一次） */
static int16_t s_win[FFT_N_POINTS];

/* ===== 内部：加窗 + Q15 输入准备 ===== */
static void build_hanning_q15(void)
{
    for (uint16_t n = 0; n < FFT_N_POINTS; ++n) {
        float w = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * n / (FFT_N_POINTS - 1)));
        int32_t q = (int32_t)(w * 32767.0f);
        if (q > 32767) q = 32767;
        s_win[n] = (int16_t)q;
    }
}

/* ADC 半满/全满回调（在 ISR 上下文），n = FFT_N_POINTS。
 * 若上一帧还没处理完就丢弃，避免竞态。 */
static void on_adc_frame(const uint16_t *raw, uint16_t n)
{
    if (n != FFT_N_POINTS || s_frame_ready) return;

    /* 12-bit ADC → Q15：先去直流，再左移 4，再乘窗 */
    int32_t sum = 0;
    for (uint16_t i = 0; i < n; ++i) sum += raw[i];
    int16_t dc = (int16_t)(sum / n);

    for (uint16_t i = 0; i < n; ++i) {
        int32_t v = ((int32_t)raw[i] - dc) << 4;
        if (v >  32767) v =  32767;
        if (v < -32768) v = -32768;
        v = (v * s_win[i]) >> 15;
        s_frame[i] = (int16_t)v;
    }
    s_frame_ready = true;
}

/* ===== 公开接口 ===== */

void fft_configure(uint32_t channel, uint32_t fs_hz)
{
    if (fft_is_running()) return;      /* 采样中禁止改配置 */
    s_channel = channel;
    s_fs_hz   = fs_hz ? fs_hz : FFT_DEFAULT_FS_HZ;
}

bool fft_is_running(void) { return ADC_IsFFTRunning(); }

bool fft_start(void)
{
    if (fft_is_running()) return true;
    memset(&s_result, 0, sizeof(s_result));
    s_result.fs_hz = s_fs_hz;
    s_frame_ready = false;
    ADC_SetFFTCallback(on_adc_frame);
    bool ok = ADC_StartFFT(s_channel, s_fs_hz, s_dma_buf,
                           (uint16_t)(FFT_N_POINTS * 2));
    if (ok) sched_set_enabled("fft", true);
    return ok;
}

void fft_stop(void)
{
    ADC_StopFFT();
    sched_set_enabled("fft", false);
}

const fft_result_t *fft_get_result(void) { return &s_result; }

/* ===== 处理一帧（fft_task 里调用） ===== */

#if USE_CMSIS_DSP
/* 找基频：跳过前 3 个 bin（直流泄漏），取最大幅值对应的频率。
 * 有效频率窗：DC~Fs/2。 */
static uint16_t find_fundamental_bin(const int16_t *mag, uint16_t n_half)
{
    uint16_t max_i = 3;
    int16_t  max_v = mag[3];
    for (uint16_t i = 4; i < n_half; ++i) {
        if (mag[i] > max_v) { max_v = mag[i]; max_i = i; }
    }
    return max_i;
}

/* THD = sqrt(H2^2 + H3^2 + ... + Hk^2) / H1，返回 % */
static float compute_thd(const int16_t *mag, uint16_t n_half,
                         uint16_t bin_f0, int max_harmonic)
{
    if (bin_f0 == 0 || mag[bin_f0] == 0) return 0.0f;
    float h1 = (float)mag[bin_f0];
    float sum_sq = 0.0f;
    for (int k = 2; k <= max_harmonic; ++k) {
        uint16_t bin = (uint16_t)(bin_f0 * k);
        if (bin >= n_half) break;
        /* 峰值可能落在相邻 bin：取窗口最大 */
        int16_t peak = mag[bin];
        if (bin + 1 < n_half && mag[bin + 1] > peak) peak = mag[bin + 1];
        if (bin >= 1         && mag[bin - 1] > peak) peak = mag[bin - 1];
        sum_sq += (float)peak * (float)peak;
    }
    return sqrtf(sum_sq) / h1 * 100.0f;
}
#endif

void fft_task(void)
{
    if (!s_frame_ready) return;

#if USE_CMSIS_DSP
    /* 输出复数谱：长度 2N（实部/虚部交错） */
    static int16_t spec[FFT_N_POINTS * 2];
    static int16_t mag [FFT_N_POINTS / 2];

    arm_rfft_q15(&s_rfft, (int16_t *)s_frame, spec);
    arm_cmplx_mag_q15(spec, mag, FFT_N_POINTS / 2);

    uint16_t bin_f0 = find_fundamental_bin(mag, FFT_N_POINTS / 2);
    s_result.f0_hz       = (float)bin_f0 * (float)s_fs_hz / (float)FFT_N_POINTS;
    s_result.thd_percent = compute_thd(mag, FFT_N_POINTS / 2, bin_f0, 5);

    /* 转 dB 给 UI 频谱页用：20*log10(mag/32768)。避免 log(0) */
    for (uint16_t i = 0; i < FFT_N_POINTS / 2; ++i) {
        float m = (float)mag[i] / 32768.0f;
        if (m < 1e-6f) m = 1e-6f;
        s_result.mag_db[i] = 20.0f * log10f(m);
    }
#endif

    s_result.frame_id++;
    s_frame_ready = false;
}

void fft_init(void)
{
    memset(&s_result, 0, sizeof(s_result));
    s_result.fs_hz = s_fs_hz;
    build_hanning_q15();

#if USE_CMSIS_DSP
    arm_rfft_init_q15(&s_rfft, FFT_N_POINTS, 0 /* forward */, 1 /* bit-reverse */);
#endif
}
