/*
 * thd.c —— THD 测量业务层
 *
 * 数据流：
 *   ADC DMA 每采满半帧（N=1024）触发 on_adc_frame（ISR 上下文）
 *     ↓ 拷贝并去直流到 s_frame，置 s_frame_ready
 *   thd_task 检测到 ready 后运行两级扫描 + Goertzel 谐波 + THD
 *     ↓
 *   thd_result_t 存到 s_result，ui/其他任务通过 thd_get_result() 读取
 *
 * 注释里的常数默认针对 Fs=40kHz、N=1024：
 *   频率分辨率 Δf = 40000/1024 ≈ 39 Hz（但 Goertzel 不受此约束）
 *   一帧观测时长 T = 1024/40000 ≈ 25.6 ms
 *   两级扫描 CPU 消耗 ≈ 100 Goertzel × 1024 sample ≈ 6 ms（无 FPU）
 */

#include "module/thd.h"
#include "module/goertzel.h"
#include "core/scheduler.h"
#include "bsp/adc.h"
#include <string.h>
#include <math.h>

/* ============================ 内部状态 ============================ */

static thd_result_t s_result;                                       /* 供外部读取 */
static uint32_t     s_channel = THD_DEFAULT_CHANNEL;
static uint32_t     s_fs_hz   = THD_DEFAULT_FS_HZ;

/* ADC DMA 目标：双帧连成一段 buf，DMA 半满/满时分别喂两半 */
static uint16_t s_dma_buf[THD_N_POINTS * 2];

/* 工作帧：ISR 里把最新一半拷贝到这里并转成 int16，供 task 处理 */
static volatile int16_t s_frame[THD_N_POINTS];
static volatile bool    s_frame_ready = false;

/* ============================ 扫描配置 ============================ */

/* 粗扫范围：50Hz 起、50Hz 步长，扫到 4kHz。保证 5*f0 < 20kHz = Fs/2。
 * 若 Fs 改到其他值，粗扫上限自动跟着 Fs/2/5 走。*/
#define COARSE_START_HZ    50.0f
#define COARSE_STEP_HZ     50.0f

/* 细扫在粗扫峰值 ± FINE_RANGE_HZ 内，FINE_STEP_HZ 步长 */
#define FINE_RANGE_HZ      50.0f
#define FINE_STEP_HZ        5.0f

/* ============================ ISR 回调 ============================ */

/* 由 bsp/adc.c 在 DMA 半满 / 满中断里调用，n = THD_N_POINTS */
static void on_adc_frame(const uint16_t *raw, uint16_t n)
{
    if (n != THD_N_POINTS) return;
    if (s_frame_ready) return;     /* 上一帧还没处理，丢弃本帧避免竞态 */

    /* 12-bit ADC 无符号 (0..4095) → int16 有符号并去直流。
     * DC 只影响 0Hz bin，我们从 50Hz 起扫描不受影响，但去掉后计算更干净。 */
    int32_t sum = 0;
    for (uint16_t i = 0; i < n; ++i) sum += raw[i];
    int16_t dc = (int16_t)(sum / n);

    for (uint16_t i = 0; i < n; ++i) {
        s_frame[i] = (int16_t)((int32_t)raw[i] - dc);
    }
    s_frame_ready = true;
}

/* ============================ 基频寻峰 ============================ */

/* 用三点抛物线插值精调峰值位置。
 * 输入：mag[c-1], mag[c], mag[c+1] 三个相邻幅度值；step_hz 是相邻点频率间隔。
 * 返回：相对于 mag[c] 中心的偏移（Hz）。
 * 原理：将三点拟合成抛物线 y = a(x-x0)² + b(x-x0) + c，求极值点。 */
static float parabolic_offset_hz(float y_prev, float y_center, float y_next,
                                 float step_hz)
{
    float denom = (y_prev - 2.0f * y_center + y_next);
    if (fabsf(denom) < 1e-6f) return 0.0f;
    /* 极值点相对中心的归一化偏移 (∈ [-0.5, 0.5])，再乘步长换算成 Hz */
    float delta = 0.5f * (y_prev - y_next) / denom;
    if (delta >  0.5f) delta =  0.5f;
    if (delta < -0.5f) delta = -0.5f;
    return delta * step_hz;
}

/* 在 [start, stop] 范围内以 step 步长做 Goertzel 扫描（幅度平方避免开根），
 * 返回峰值下标（相对 start 的整数索引）和峰值前/中/后 3 个幅度值以便插值。 */
static uint32_t goertzel_sweep_argmax(const int16_t *samples, uint32_t n,
                                      uint32_t fs_hz,
                                      float start_hz, float step_hz,
                                      uint32_t n_steps,
                                      float *out_prev, float *out_peak,
                                      float *out_next)
{
    uint32_t peak_idx = 0;
    float    peak_val = -1.0f;
    float    prev_val = 0.0f, next_val = 0.0f;

    /* 保留上一格的幅度，方便 3 点抛物线用 */
    float last_mag2 = 0.0f;

    for (uint32_t i = 0; i < n_steps; ++i) {
        float f = start_hz + (float)i * step_hz;
        if (f >= (float)fs_hz * 0.5f) break;      /* Nyquist 保护 */
        float m2 = goertzel_magnitude2(samples, n, fs_hz, f);

        if (m2 > peak_val) {
            peak_val = m2;
            peak_idx = i;
            prev_val = last_mag2;   /* 保存上一格作为 peak 的 prev */
            next_val = 0.0f;        /* 新峰重置 next，等下一格填 */
        }
        /* 若当前是 peak+1，此时的 m2 就是 next */
        if (i == peak_idx + 1) {
            next_val = m2;
        }
        last_mag2 = m2;
    }

    if (out_prev) *out_prev = prev_val;
    if (out_peak) *out_peak = peak_val;
    if (out_next) *out_next = next_val;
    return peak_idx;
}

/* 两级扫描：粗扫 → 细扫 → 抛物线，返回估计的 f0 */
static float estimate_f0(const int16_t *samples, uint32_t n, uint32_t fs_hz)
{
    /* -------- 粗扫 -------- */
    /* 上限：Fs/2/5 保证 5 次谐波不越 Nyquist。取 min(4000, Fs/10) */
    float coarse_stop = (float)fs_hz / 10.0f;
    if (coarse_stop > 4000.0f) coarse_stop = 4000.0f;
    uint32_t coarse_n = (uint32_t)((coarse_stop - COARSE_START_HZ) / COARSE_STEP_HZ);
    if (coarse_n < 3) coarse_n = 3;

    float cp = 0, cc = 0, cn = 0;
    uint32_t ci = goertzel_sweep_argmax(samples, n, fs_hz,
                                        COARSE_START_HZ, COARSE_STEP_HZ,
                                        coarse_n, &cp, &cc, &cn);
    float f_coarse = COARSE_START_HZ + (float)ci * COARSE_STEP_HZ;

    /* -------- 细扫（在 f_coarse ± FINE_RANGE_HZ 里以 FINE_STEP_HZ 步长扫）-------- */
    float fine_start = f_coarse - FINE_RANGE_HZ;
    if (fine_start < COARSE_START_HZ) fine_start = COARSE_START_HZ;
    uint32_t fine_n = (uint32_t)((2 * FINE_RANGE_HZ) / FINE_STEP_HZ) + 1;

    float fp = 0, fc = 0, fn = 0;
    uint32_t fi = goertzel_sweep_argmax(samples, n, fs_hz,
                                        fine_start, FINE_STEP_HZ,
                                        fine_n, &fp, &fc, &fn);
    float f_fine = fine_start + (float)fi * FINE_STEP_HZ;

    /* -------- 抛物线插值精调 -------- */
    /* 如果 fp / fn 拿不到（在边界），插值退化为 0，直接返回 f_fine */
    float delta = parabolic_offset_hz(fp, fc, fn, FINE_STEP_HZ);
    return f_fine + delta;
}

/* ============================ 谐波与 THD ============================ */

static void compute_harmonics_and_thd(const int16_t *samples, uint32_t n,
                                      uint32_t fs_hz, float f0,
                                      thd_result_t *out)
{
    float nyq = (float)fs_hz * 0.5f;

    /* H1..H5：一次 Goertzel 复数结果 → 同时得到幅度 + 相位 */
    for (int k = 1; k <= THD_MAX_HARMONIC; ++k) {
        float f = f0 * (float)k;
        if (f >= nyq) {
            out->harmonic[k - 1]       = 0.0f;
            out->harmonic_phase[k - 1] = 0.0f;
        } else {
            goertzel_complex_t c = goertzel_complex(samples, n, fs_hz, f);
            out->harmonic[k - 1]       = sqrtf(c.re * c.re + c.im * c.im);
            out->harmonic_phase[k - 1] = atan2f(c.im, c.re);
        }
    }

    /* THD = sqrt(H2²+H3²+...+H5²) / H1 × 100 */
    float h1 = out->harmonic[0];
    if (h1 < 1e-3f) {
        out->thd_percent = 0.0f;    /* 无有效基频，避免除零 */
        return;
    }
    float sum_sq = 0.0f;
    for (int k = 2; k <= THD_MAX_HARMONIC; ++k) {
        float h = out->harmonic[k - 1];
        sum_sq += h * h;
    }
    out->thd_percent = sqrtf(sum_sq) / h1 * 100.0f;
}

/* ============================ 公开接口 ============================ */

bool thd_is_running(void) { return ADC_IsFFTRunning(); }

void thd_configure(uint32_t channel, uint32_t fs_hz)
{
    if (thd_is_running()) return;   /* 采样中禁止改 */
    s_channel = channel;
    s_fs_hz   = fs_hz ? fs_hz : THD_DEFAULT_FS_HZ;
}

bool thd_start(void)
{
    if (thd_is_running()) return true;
    memset(&s_result, 0, sizeof(s_result));
    s_result.fs_hz = s_fs_hz;
    s_frame_ready  = false;

    ADC_SetFFTCallback(on_adc_frame);
    bool ok = ADC_StartFFT(s_channel, s_fs_hz, s_dma_buf,
                           (uint16_t)(THD_N_POINTS * 2));
    if (ok) sched_set_enabled("thd", true);
    return ok;
}

void thd_stop(void)
{
    ADC_StopFFT();
    sched_set_enabled("thd", false);
}

const thd_result_t *thd_get_result(void) { return &s_result; }

/* ============================ 任务函数 ============================ */

void thd_task(void)
{
    if (!s_frame_ready) return;

    /* 复制 volatile 数组的指针可以直接给 const int16_t* 使用 */
    const int16_t *samples = (const int16_t *)s_frame;

    float f0 = estimate_f0(samples, THD_N_POINTS, s_fs_hz);

    thd_result_t r;
    memset(&r, 0, sizeof(r));
    r.f0_hz    = f0;
    r.fs_hz    = s_fs_hz;
    r.frame_id = s_result.frame_id + 1;

    compute_harmonics_and_thd(samples, THD_N_POINTS, s_fs_hz, f0, &r);

    s_result = r;         /* 结构体整体赋值 = 原子发布（相对协作任务） */
    s_frame_ready = false;
}

/* ============================ 初始化 ============================ */

void thd_init(void)
{
    memset(&s_result, 0, sizeof(s_result));
    s_result.fs_hz = s_fs_hz;

}
