/*
 * learning.c —— 未知电路学习算法 (发挥部分 (1))
 *
 * 算法链:
 *   [DDS 扫频]  →  [ADC 采样]  →  [Goertzel 提幅]  →  [归一化幅频表]
 *                                                         ↓
 *                                    [四类判别]  →  [Gauss-Newton 拟合]
 *                                                         ↓
 *                                                   learn_result_t
 *
 * 状态机由 learning_task() 每 10 ms 推进一次, 单次动作 << 5 ms.
 * ADC 与 DDS 由 bsp/adc + module/dds 提供, 本模块只做业务逻辑.
 */

#include "module/learning.h"
#include "module/dds.h"
#include "module/goertzel.h"
#include "bsp/adc.h"
#include "bsp/uart.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ==================================================================
 *  可调参数
 * ================================================================== */
#define LEARN_N_POINTS       60          /* 扫频点数 (对数分布) */
#define LEARN_F_MIN_HZ       500.0f      /* 扫频下限 */
#define LEARN_F_MAX_HZ       200000.0f   /* 扫频上限 (覆盖 5k~50k RLC 谐振区) */
#define LEARN_DDS_VPP        0.6f        /* 每点激励峰峰值 */
#define LEARN_ADC_FS_HZ      500000UL    /* 采样率 500 kSPS */
#define LEARN_ADC_CH         ADC_CHANNEL_1
#define LEARN_ADC_BUF_LEN    1024U       /* 每点采样长度 (必偶数) */
#define LEARN_SETTLE_MS      15          /* 换频后瞬态衰减 */
#define LEARN_CAPTURE_MS     20          /* ADC 采集超时 */
#define LEARN_TASK_TICK_MS   10          /* 与调度器周期一致 */

#define TWO_PI               6.28318530717958647692f

/* ==================================================================
 *  内部状态
 * ================================================================== */
typedef enum {
    LP_IDLE = 0,
    LP_START,
    LP_SET_FREQ,
    LP_SETTLING,
    LP_CAPTURING,
    LP_COMPUTING,
    LP_CLASSIFY,
    LP_FIT,
} learn_phase_t;

static learn_state_t   s_state  = LEARN_IDLE;
static learn_result_t  s_result = { FILTER_UNKNOWN, {0}, 0 };
static learn_phase_t   s_phase  = LP_IDLE;

static float           s_freq_tab[LEARN_N_POINTS];   /* Hz */
static float           s_h_mag  [LEARN_N_POINTS];    /* |H(f_i)| 原始幅度 */

static uint16_t        s_adc_buf[LEARN_ADC_BUF_LEN];
static volatile bool   s_capture_done = false;

static uint16_t        s_idx     = 0;
static uint32_t        s_tick_ms = 0;

/* ==================================================================
 *  数学: 模型幅频响应 |H(f)| (参见 report 2.3 节)
 * ================================================================== */
static float model_mag(filter_type_t t, float w0, float zeta, float K, float f)
{
    float w    = TWO_PI * f;
    float ww   = w * w;
    float w0w0 = w0 * w0;
    float re   = w0w0 - ww;
    float im   = 2.0f * zeta * w0 * w;
    float denom = sqrtf(re * re + im * im);
    if (denom < 1e-12f) denom = 1e-12f;

    switch (t) {
    case FILTER_LOWPASS:  return K * w0w0 / denom;
    case FILTER_HIGHPASS: return K * ww   / denom;
    case FILTER_BANDPASS: return K * im   / denom;
    case FILTER_BANDSTOP: return K * fabsf(re) / denom;
    default:              return 0.0f;
    }
}

/* ==================================================================
 *  幅频曲线 → 滤波类型 (report 2.3 节判据)
 * ================================================================== */
static filter_type_t classify(const float *h, uint16_t n)
{
    /* 找峰值 */
    uint16_t i_max = 0;
    for (uint16_t i = 1; i < n; ++i)
        if (h[i] > h[i_max]) i_max = i;
    float hmax = h[i_max];
    if (hmax < 1e-6f) return FILTER_UNKNOWN;

    /* 归一化两端 */
    float h_lo       = h[0]     / hmax;
    float h_hi       = h[n - 1] / hmax;
    float peak_pos   = (float)i_max / (float)(n - 1);   /* 0..1 */

    const float HI = 0.7f;
    const float LO = 0.3f;

    if (h_lo > HI && h_hi < LO) return FILTER_LOWPASS;
    if (h_lo < LO && h_hi > HI) return FILTER_HIGHPASS;
    if (h_lo < LO && h_hi < LO &&
        peak_pos > 0.15f && peak_pos < 0.85f)
        return FILTER_BANDPASS;
    if (h_lo > HI && h_hi > HI) {
        /* 中间是否存在极小值 → 带阻 */
        uint16_t i_min = 0;
        for (uint16_t i = 1; i < n; ++i)
            if (h[i] < h[i_min]) i_min = i;
        float dip_pos = (float)i_min / (float)(n - 1);
        float dip_ratio = h[i_min] / hmax;
        if (dip_pos > 0.15f && dip_pos < 0.85f && dip_ratio < 0.5f)
            return FILTER_BANDSTOP;
    }
    return FILTER_UNKNOWN;
}

/* ==================================================================
 *  Gauss-Newton 拟合  (ω0, ζ, K)
 * ================================================================== */
static void fit_params(filter_type_t t,
                       const float *freq, const float *hmag, uint16_t n,
                       float *out_w0, float *out_zeta, float *out_K)
{
    /* --- 初值 --- */
    float hmax = 0.0f;
    uint16_t i_max = 0;
    for (uint16_t i = 0; i < n; ++i)
        if (hmag[i] > hmax) { hmax = hmag[i]; i_max = i; }

    float K    = hmax;
    float zeta = 0.5f;
    float w0   = TWO_PI * freq[n / 2];

    switch (t) {
    case FILTER_BANDPASS:
        w0 = TWO_PI * freq[i_max];
        break;
    case FILTER_BANDSTOP: {
        uint16_t i_min = 0;
        for (uint16_t i = 1; i < n; ++i)
            if (hmag[i] < hmag[i_min]) i_min = i;
        w0 = TWO_PI * freq[i_min];
        break; }
    case FILTER_LOWPASS:
        for (uint16_t i = 0; i < n; ++i)
            if (hmag[i] < K * 0.707f) { w0 = TWO_PI * freq[i]; break; }
        break;
    case FILTER_HIGHPASS:
        for (uint16_t i = n; i > 0; --i)
            if (hmag[i - 1] < K * 0.707f) { w0 = TWO_PI * freq[i - 1]; break; }
        break;
    default: break;
    }

    /* --- Gauss-Newton (3 未知量) --- */
    const int   MAX_ITER = 15;
    const float EPS_W = 1e-3f, EPS_Z = 1e-4f, EPS_K = 1e-4f;

    for (int iter = 0; iter < MAX_ITER; ++iter) {
        float JtJ[3][3] = {{0,0,0},{0,0,0},{0,0,0}};
        float Jtr[3]    = {0,0,0};

        for (uint16_t i = 0; i < n; ++i) {
            float h0 = model_mag(t, w0,                    zeta, K, freq[i]);
            float hp = model_mag(t, w0 * (1.0f + EPS_W),   zeta, K, freq[i]);
            float hz = model_mag(t, w0, zeta + EPS_Z,      K, freq[i]);
            float hk = model_mag(t, w0, zeta, K + EPS_K,      freq[i]);

            float J0 = (hp - h0) / (w0 * EPS_W);
            float J1 = (hz - h0) / EPS_Z;
            float J2 = (hk - h0) / EPS_K;
            float r  = hmag[i] - h0;

            JtJ[0][0] += J0*J0; JtJ[0][1] += J0*J1; JtJ[0][2] += J0*J2;
            JtJ[1][0] += J1*J0; JtJ[1][1] += J1*J1; JtJ[1][2] += J1*J2;
            JtJ[2][0] += J2*J0; JtJ[2][1] += J2*J1; JtJ[2][2] += J2*J2;
            Jtr[0]    += J0*r;  Jtr[1]    += J1*r;  Jtr[2]    += J2*r;
        }

        /* 解 3×3: Cramer */
        float det =
              JtJ[0][0]*(JtJ[1][1]*JtJ[2][2] - JtJ[1][2]*JtJ[2][1])
            - JtJ[0][1]*(JtJ[1][0]*JtJ[2][2] - JtJ[1][2]*JtJ[2][0])
            + JtJ[0][2]*(JtJ[1][0]*JtJ[2][1] - JtJ[1][1]*JtJ[2][0]);
        if (fabsf(det) < 1e-15f) break;

        float dp0 = ( Jtr[0]*(JtJ[1][1]*JtJ[2][2] - JtJ[1][2]*JtJ[2][1])
                    - JtJ[0][1]*(Jtr[1]*JtJ[2][2] - JtJ[1][2]*Jtr[2])
                    + JtJ[0][2]*(Jtr[1]*JtJ[2][1] - JtJ[1][1]*Jtr[2])) / det;
        float dp1 = ( JtJ[0][0]*(Jtr[1]*JtJ[2][2] - JtJ[1][2]*Jtr[2])
                    - Jtr[0]*(JtJ[1][0]*JtJ[2][2] - JtJ[1][2]*JtJ[2][0])
                    + JtJ[0][2]*(JtJ[1][0]*Jtr[2] - Jtr[1]*JtJ[2][0])) / det;
        float dp2 = ( JtJ[0][0]*(JtJ[1][1]*Jtr[2] - Jtr[1]*JtJ[2][1])
                    - JtJ[0][1]*(JtJ[1][0]*Jtr[2] - Jtr[1]*JtJ[2][0])
                    + Jtr[0]*(JtJ[1][0]*JtJ[2][1] - JtJ[1][1]*JtJ[2][0])) / det;

        w0   += dp0;
        zeta += dp1;
        K    += dp2;

        if (zeta < 0.02f) zeta = 0.02f;
        if (zeta > 10.0f) zeta = 10.0f;
        if (w0   < 1.0f)  w0   = 1.0f;

        if (fabsf(dp0 / w0) < 1e-3f &&
            fabsf(dp1)      < 1e-4f &&
            fabsf(dp2 / (K + 1e-9f)) < 1e-3f) break;
    }

    *out_w0   = w0;
    *out_zeta = zeta;
    *out_K    = K;
}

/* ==================================================================
 *  辅助
 * ================================================================== */
static void freq_table_init(void)
{
    float log_min = logf(LEARN_F_MIN_HZ);
    float log_max = logf(LEARN_F_MAX_HZ);
    float step    = (log_max - log_min) / (float)(LEARN_N_POINTS - 1);
    for (uint16_t i = 0; i < LEARN_N_POINTS; ++i)
        s_freq_tab[i] = expf(log_min + step * (float)i);
}

static void on_adc_ready(const uint16_t *raw, uint16_t n)
{
    (void)raw; (void)n;
    s_capture_done = true;    /* DMA 半满/全满都视为一段数据就绪 */
}

/* ==================================================================
 *  生命周期 API
 * ================================================================== */
void learning_init(void)
{
    s_state = LEARN_IDLE;
    s_phase = LP_IDLE;
    memset(&s_result, 0, sizeof(s_result));
    s_result.type = FILTER_UNKNOWN;
    freq_table_init();
}

bool learning_start(void)
{
    if (s_state == LEARN_RUNNING) return false;
    memset(s_h_mag, 0, sizeof(s_h_mag));
    s_idx     = 0;
    s_tick_ms = 0;
    s_state   = LEARN_RUNNING;
    s_phase   = LP_START;
    UART_Printf("[learn] start: %u points, %.0f -> %.0f Hz\r\n",
                (unsigned)LEARN_N_POINTS,
                (double)LEARN_F_MIN_HZ, (double)LEARN_F_MAX_HZ);
    return true;
}

void learning_stop(void)
{
    ADC_StopFFT();
    dds_stop();
    s_state = LEARN_IDLE;
    s_phase = LP_IDLE;
}

learn_state_t         learning_get_state (void) { return s_state; }
const learn_result_t *learning_get_result(void) { return &s_result; }
void learning_set_result(const learn_result_t *r) { if (r) s_result = *r; }
void learning_set_state (learn_state_t s)         { s_state = s; }

const char *learning_filter_name(filter_type_t t)
{
    switch (t) {
    case FILTER_LOWPASS:  return "LOWPASS";
    case FILTER_HIGHPASS: return "HIGHPASS";
    case FILTER_BANDPASS: return "BANDPASS";
    case FILTER_BANDSTOP: return "BANDSTOP";
    case FILTER_ALLPASS:  return "ALLPASS";
    default:              return "UNKNOWN";
    }
}

/* ==================================================================
 *  学习状态机 (10 ms tick 推进)
 * ================================================================== */
void learning_task(void)
{
    if (s_state != LEARN_RUNNING) return;
    s_tick_ms += LEARN_TASK_TICK_MS;

    switch (s_phase) {
    case LP_START:
        ADC_SetFFTCallback(on_adc_ready);
        s_idx     = 0;
        s_tick_ms = 0;
        s_phase   = LP_SET_FREQ;
        break;

    case LP_SET_FREQ:
        dds_tone_sine((double)s_freq_tab[s_idx], LEARN_DDS_VPP, 0.0f);
        s_capture_done = false;
        s_tick_ms      = 0;
        s_phase        = LP_SETTLING;
        break;

    case LP_SETTLING:
        if (s_tick_ms >= LEARN_SETTLE_MS) {
            ADC_StartFFT(LEARN_ADC_CH, LEARN_ADC_FS_HZ,
                         s_adc_buf, LEARN_ADC_BUF_LEN);
            s_tick_ms = 0;
            s_phase   = LP_CAPTURING;
        }
        break;

    case LP_CAPTURING:
        if (s_capture_done || s_tick_ms >= LEARN_CAPTURE_MS) {
            ADC_StopFFT();
            s_phase = LP_COMPUTING;
        }
        break;

    case LP_COMPUTING: {
        /* 12-bit ADC 中值 2048, 转 int16 (含符号) 后送 Goertzel */
        static int16_t s16buf[LEARN_ADC_BUF_LEN];
        for (uint16_t i = 0; i < LEARN_ADC_BUF_LEN; ++i)
            s16buf[i] = (int16_t)((int32_t)s_adc_buf[i] - 2048);

        float mag = goertzel_magnitude(s16buf, LEARN_ADC_BUF_LEN,
                                       LEARN_ADC_FS_HZ, s_freq_tab[s_idx]);
        s_h_mag[s_idx] = mag;

        if (++s_idx >= LEARN_N_POINTS) {
            s_phase = LP_CLASSIFY;
        } else {
            s_phase = LP_SET_FREQ;
        }
        break; }

    case LP_CLASSIFY:
        s_result.type = classify(s_h_mag, LEARN_N_POINTS);
        UART_Printf("[learn] classify -> %s\r\n",
                    learning_filter_name(s_result.type));
        s_phase = LP_FIT;
        break;

    case LP_FIT: {
        if (s_result.type == FILTER_UNKNOWN) {
            dds_stop();
            s_state = LEARN_FAILED;
            s_phase = LP_IDLE;
            UART_Printf("[learn] FAILED: cannot classify\r\n");
            break;
        }
        float w0, zeta, K;
        fit_params(s_result.type, s_freq_tab, s_h_mag, LEARN_N_POINTS,
                   &w0, &zeta, &K);
        s_result.params[0] = w0;
        s_result.params[1] = zeta;
        s_result.params[2] = K;
        s_result.params[3] = 0.0f;
        s_result.n_params  = 3;

        dds_stop();
        s_state = LEARN_DONE;
        s_phase = LP_IDLE;
        UART_Printf("[learn] DONE: type=%s, f0=%.1f Hz, zeta=%.3f, K=%.3f\r\n",
                    learning_filter_name(s_result.type),
                    (double)(w0 / TWO_PI), (double)zeta, (double)K);
        break; }

    default:
        break;
    }
}
