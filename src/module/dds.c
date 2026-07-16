/*
 * dds.c —— DDS 输出模块 (module 层，业务侧)
 *
 * 见 dds.h 头注释。本文件按 5 种模式分节：
 *    §1  内部状态与工具
 *    §2  Mode 1  单音
 *    §3  Mode 2  任意波 (RAM)
 *    §4  Mode 3  扫频 (DRG)
 *    §5  Mode 4  跳频 (Multi-Profile)
 *    §6  Mode 5  OSK
 *    §7  任务驱动 dds_task
 *    §8  init / dump
 */

#include "module/dds.h"
#include "drv/ad9910.h"
#include "bsp/uart.h"
#include "stm32f1xx_hal.h"
#include <math.h>
#include <string.h>

/* ==================================================================
 *  §1  内部状态
 * ================================================================== */

static dds_mode_t s_mode = DDS_MODE_IDLE;

/* --- Mode 3 扫频状态机 --- */
typedef struct {
    dds_sweep_style_t style;
    bool     running;
    uint8_t  dest;              /* 0=freq, 1=amp, 2=phase */
    /* 记录一份原始参数，方便 dump / 重启 */
    double   f_start_hz, f_stop_hz;
    float    v_start,    v_stop;
    float    deg_start,  deg_stop;
    float    sweep_time_s;
} sweep_state_t;
static sweep_state_t s_sweep;

/* --- Mode 4 跳频状态机 --- */
typedef struct {
    uint8_t  n;                 /* 已加载 profile 数 (≤8)               */
    uint8_t  idx;               /* 当前 profile 下标                    */
    int8_t   dir;               /* +1/-1，PING_PONG 用                  */
    dds_hop_mode_t mode;
    uint32_t period_ms;
    uint32_t last_ms;
    /* 保留一份用户参数，方便 dump */
    double   freq_list[8];
    float    amp_list[8];
} hop_state_t;
static hop_state_t s_hop;

/* --- amp_v ↔ ASF 归一化换算 --- */
static float amp_v_to_amp01(float amp_v)
{
    if (amp_v < 0.0f) amp_v = 0.0f;
    float a = amp_v / DDS_FULL_SCALE_V;
    if (a > 1.0f) a = 1.0f;
    return a;
}

/* ==================================================================
 *  §2  Mode 1 · 单音
 * ================================================================== */

/* 切进/离开非单音模式时，需要复位 CFR (关掉 RAM / DRG)。
 * 简化：所有"改模式"的 API 里都调用一次这个，保证从任何模式回单音干净。 */
static void enter_single_tone_mode(void)
{
    ad9910_ram_disable();       /* CFR1 = 0 */
    ad9910_drg_disable();       /* CFR2 = base */
    ad9910_profile_select(0);
    s_mode = DDS_MODE_TONE;
}

void dds_tone_sine(double f_hz, float amp_v, float phase_deg)
{
    float amp01 = amp_v_to_amp01(amp_v);
    uint32_t ftw  = ad9910_freq_to_ftw    (f_hz);
    uint16_t asf  = ad9910_amp01_to_asf   (amp01);
    uint16_t pow16= ad9910_phase_deg_to_pow(phase_deg);

    UART_Printf("[dds] tone_sine: f=%.3f Hz  amp_v=%.3f V  phase=%.2f deg\r\n",
                f_hz, (double)amp_v, (double)phase_deg);
    UART_Printf("[dds]   -> amp01=%.4f (硬饱和后)  FTW=0x%08lX  ASF=0x%04X  POW=0x%04X\r\n",
                (double)amp01, (unsigned long)ftw, (unsigned)asf, (unsigned)pow16);
    if (amp_v > DDS_FULL_SCALE_V) {
        UART_Printf("[dds]   ! amp_v > DDS_FULL_SCALE_V(%.2fV), hard-clipped to full scale\r\n",
                    (double)DDS_FULL_SCALE_V);
    }

    enter_single_tone_mode();
    ad9910_tone_t t = {
        .f_hz = f_hz, .amp01 = amp01, .phase_deg = phase_deg,
    };
    ad9910_set_tone(&t);
}

void dds_tone_freq (double f_hz)   { UART_Printf("[dds] tone_freq  = %.3f Hz\r\n", f_hz);      enter_single_tone_mode(); ad9910_set_freq_hz(f_hz); }
void dds_tone_amp  (float  amp_v)  { UART_Printf("[dds] tone_amp   = %.3f V\r\n", (double)amp_v); enter_single_tone_mode(); ad9910_set_amp01(amp_v_to_amp01(amp_v)); }
void dds_tone_phase(float  deg)    { UART_Printf("[dds] tone_phase = %.2f deg\r\n", (double)deg); enter_single_tone_mode(); ad9910_set_phase_deg(deg); }

/* ==================================================================
 *  §3  Mode 2 · 任意波 (RAM playback，Polar 模式)
 *
 *  样点编码 (Polar dest, datasheet Table 20):
 *    RAM 每字 32-bit:
 *      [31:18] ASF 14-bit  (幅度)
 *      [17:2 ] POW 16-bit  (相位)
 *      [ 1:0 ] Reserved
 *
 *  简化：只用幅度调制 (POW=0)，即 ASF-only。这样也能得到方波/三角波，
 *  且不用担心相位跳变引入的过冲。
 *
 *  播放速率：AD9910 RAM step 时钟 = SYSCLK/4 = 250 MHz，
 *    每步经过 (rate_divider+1) 个 SYSCLK/4 时钟。
 *    波形频率 f = 250e6 / (rate_divider+1) / N。
 *  → rate_divider = round(250e6 / (f × N)) - 1  (16-bit，0..65535)
 * ================================================================== */

static uint32_t s_arb_ram[DDS_ARB_RAM_DEPTH];   /* 组包缓冲 */

/* 把 -1..+1 的 float 样点编成 RAM Polar 字 (仅用幅度，正数直接 ASF；
 * 负数用 POW=180° 转成正 ASF)。整体乘以 amp_v 缩放。*/
static uint32_t sample_to_ram_word(float s, float amp01)
{
    float mag = s * amp01;
    /* 幅度极性：负 → 相位翻转 180° */
    uint16_t pow16 = 0x0000;
    if (mag < 0.0f) {
        mag = -mag;
        pow16 = 0x8000;   /* 180° */
    }
    if (mag > 1.0f) mag = 1.0f;
    uint16_t asf14 = (uint16_t)(mag * 16383.0f + 0.5f);
    /* Polar 字位分配: [31:18]=ASF, [17:2]=POW */
    return ((uint32_t)(asf14 & 0x3FFF) << 18) | ((uint32_t)pow16 << 2);
}

static bool arb_load_ram_and_start(uint16_t n, double freq_hz, float amp01)
{
    if (n == 0 || n > DDS_ARB_RAM_DEPTH) return false;
    if (freq_hz <= 0.0) return false;

    /* --- 计算 rate_divider --- */
    /* SYSCLK/4 = 250 MHz */
    double step_clk = (double)AD9910_SYSCLK_HZ / 4.0;
    double div      = step_clk / (freq_hz * (double)n) - 1.0;
    if (div < 0.0) div = 0.0;
    if (div > 65535.0) div = 65535.0;   /* 频率过低会饱和；用户需增加 n 或降 sysclk */
    uint16_t rate_div = (uint16_t)(div + 0.5);

    /* --- 切模式 (确保 DRG 关掉，profile 引脚归 0) --- */
    ad9910_drg_disable();
    ad9910_profile_select(0);

    /* --- 写 RAM --- */
    ad9910_ram_write(0, s_arb_ram, n);

    /* --- 配置 profile 0 = RAM playback 参数 --- */
    ad9910_ram_profile_t cfg = {
        .start_addr    = 0,
        .end_addr      = (uint16_t)(n - 1),
        .rate_divider  = rate_div,
        .mode          = AD9910_RAM_MODE_CONTINUOUS_RAMP,
        .no_dwell_high = false,
        .zero_crossing = false,
    };
    ad9910_ram_profile_config(0, &cfg);

    /* --- 使能 RAM (Polar 目标) --- */
    ad9910_ram_enable(AD9910_CFR1_RAM_DEST_POLAR);
    s_mode = DDS_MODE_ARB;
    (void)amp01;    /* amp01 已经在样点里体现了 */
    return true;
}

bool dds_arb_load(const float *samples, uint16_t n, double freq_hz, float amp_v)
{
    UART_Printf("[dds] arb_load: n=%u  f=%.3f Hz  amp=%.3f V\r\n",
                (unsigned)n, freq_hz, (double)amp_v);
    if (!samples) return false;
    float amp01 = amp_v_to_amp01(amp_v);
    for (uint16_t i = 0; i < n; ++i) s_arb_ram[i] = sample_to_ram_word(samples[i], amp01);
    return arb_load_ram_and_start(n, freq_hz, amp01);
}

bool dds_arb_from_fn(dds_arb_fn_t fn, uint16_t n, double freq_hz, float amp_v)
{
    UART_Printf("[dds] arb_from_fn: n=%u  f=%.3f Hz  amp=%.3f V\r\n",
                (unsigned)n, freq_hz, (double)amp_v);
    if (!fn || n == 0) return false;
    float amp01 = amp_v_to_amp01(amp_v);
    for (uint16_t i = 0; i < n; ++i) {
        float t = (float)i / (float)n;
        s_arb_ram[i] = sample_to_ram_word(fn(t), amp01);
    }
    return arb_load_ram_and_start(n, freq_hz, amp01);
}

/* --- 常用波形回调 --- */
static float wf_sine(float t)      { return sinf(6.28318530717958647692f * t); }
static float wf_triangle(float t)  { return (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t); }
static float wf_sawtooth(float t)  { return 2.0f * t - 1.0f; }

/* square 需要 duty，用静态参数传 —— dds_arb_from_fn 的 fn 不带 user_data */
static float s_square_duty = 0.5f;
static float wf_square(float t)    { return (t < s_square_duty) ? 1.0f : -1.0f; }

bool dds_arb_sine    (double f, float v) { return dds_arb_from_fn(wf_sine,     DDS_ARB_RAM_DEPTH, f, v); }
bool dds_arb_triangle(double f, float v) { return dds_arb_from_fn(wf_triangle, DDS_ARB_RAM_DEPTH, f, v); }
bool dds_arb_sawtooth(double f, float v) { return dds_arb_from_fn(wf_sawtooth, DDS_ARB_RAM_DEPTH, f, v); }
bool dds_arb_square  (double f, float duty, float v)
{
    if (duty < 0.01f) duty = 0.01f;
    if (duty > 0.99f) duty = 0.99f;
    s_square_duty = duty;
    return dds_arb_from_fn(wf_square, DDS_ARB_RAM_DEPTH, f, v);
}

/* ==================================================================
 *  §4  Mode 3 · 扫频 (DRG)
 *
 *  DRG 速率换算 (datasheet):
 *    step_clk       = SYSCLK / 4 = 250 MHz
 *    每 (pos_rate+1) 个 step_clk 走一次 incr_step
 *    扫完 (upper-lower) 用时 T = (upper-lower)/incr_step × (pos_rate+1)/step_clk
 *
 *  策略：把 pos_rate 固定为 1，把 incr_step 由 T 反算出来。这样对大部分
 *  扫描时间来说精度都足够。
 * ================================================================== */

static void drg_build_cfg(uint32_t lower32, uint32_t upper32,
                          float sweep_time_s, dds_sweep_style_t style,
                          ad9910_drg_dest_t dest,
                          ad9910_drg_cfg_t *out)
{
    /* 保证 lower < upper */
    if (upper32 < lower32) { uint32_t t = lower32; lower32 = upper32; upper32 = t; }
    if (sweep_time_s <= 0.0f) sweep_time_s = 0.001f;

    uint64_t range   = (uint64_t)(upper32 - lower32);
    double   step_clk = (double)AD9910_SYSCLK_HZ / 4.0;
    /* 每次 tick 增量: incr = range / (sweep_time_s * step_clk) */
    double   incr_d  = (double)range / ((double)sweep_time_s * step_clk);
    if (incr_d < 1.0) incr_d = 1.0;
    uint32_t incr    = (incr_d > 4294967295.0) ? 0xFFFFFFFFUL : (uint32_t)incr_d;

    out->dest       = dest;
    out->lower      = lower32;
    out->upper      = upper32;
    out->incr_step  = incr;
    out->decr_step  = incr;    /* 双向扫时反向速率也一样 */
    out->pos_rate   = 1;
    out->neg_rate   = 1;
    /* style → no-dwell 位:
     *   ONESHOT   : no-dwell 全 0，DRG 到 upper 停住
     *   BIDIR     : no-dwell 全 0 (需要靠 DRHOLD/DRCTL 引脚控制方向) —— 未接
     *   CONT_UP   : no-dwell high = 1，到 upper 立即跳回 lower
     * 注：完美 BIDIR 需要 DRHOLD/DRCTL 引脚；本板未接，此处近似退化为 CONT_UP */
    out->no_dwell_high = (style != DDS_SWEEP_ONESHOT);
    out->no_dwell_low  = false;
}

bool dds_sweep_freq(double f_start, double f_stop, float t_s, dds_sweep_style_t style)
{
    UART_Printf("[dds] sweep_freq: %.3f -> %.3f Hz in %.3f s, style=%d\r\n",
                f_start, f_stop, (double)t_s, (int)style);
    enter_single_tone_mode();          /* 先回单音，再进 DRG */
    uint32_t lo = ad9910_freq_to_ftw(f_start);
    uint32_t hi = ad9910_freq_to_ftw(f_stop);
    ad9910_drg_cfg_t cfg;
    drg_build_cfg(lo, hi, t_s, style, AD9910_DRG_DEST_FTW, &cfg);
    ad9910_drg_configure(&cfg);

    s_sweep.style        = style;
    s_sweep.dest         = 0;
    s_sweep.f_start_hz   = f_start;
    s_sweep.f_stop_hz    = f_stop;
    s_sweep.sweep_time_s = t_s;
    s_sweep.running      = true;
    s_mode = DDS_MODE_SWEEP;
    return true;
}

bool dds_sweep_amp(float v_start, float v_stop, float t_s, dds_sweep_style_t style)
{
    UART_Printf("[dds] sweep_amp: %.3f -> %.3f V in %.3f s, style=%d\r\n",
                (double)v_start, (double)v_stop, (double)t_s, (int)style);
    enter_single_tone_mode();
    uint32_t lo = (uint32_t)ad9910_amp01_to_asf(amp_v_to_amp01(v_start)) << 18;
    uint32_t hi = (uint32_t)ad9910_amp01_to_asf(amp_v_to_amp01(v_stop))  << 18;
    ad9910_drg_cfg_t cfg;
    drg_build_cfg(lo, hi, t_s, style, AD9910_DRG_DEST_ASF, &cfg);
    ad9910_drg_configure(&cfg);

    s_sweep.style        = style;
    s_sweep.dest         = 1;
    s_sweep.v_start      = v_start;
    s_sweep.v_stop       = v_stop;
    s_sweep.sweep_time_s = t_s;
    s_sweep.running      = true;
    s_mode = DDS_MODE_SWEEP;
    return true;
}

bool dds_sweep_phase(float d_start, float d_stop, float t_s, dds_sweep_style_t style)
{
    UART_Printf("[dds] sweep_phase: %.2f -> %.2f deg in %.3f s, style=%d\r\n",
                (double)d_start, (double)d_stop, (double)t_s, (int)style);
    enter_single_tone_mode();
    uint32_t lo = (uint32_t)ad9910_phase_deg_to_pow(d_start) << 16;
    uint32_t hi = (uint32_t)ad9910_phase_deg_to_pow(d_stop)  << 16;
    ad9910_drg_cfg_t cfg;
    drg_build_cfg(lo, hi, t_s, style, AD9910_DRG_DEST_POW, &cfg);
    ad9910_drg_configure(&cfg);

    s_sweep.style        = style;
    s_sweep.dest         = 2;
    s_sweep.deg_start    = d_start;
    s_sweep.deg_stop     = d_stop;
    s_sweep.sweep_time_s = t_s;
    s_sweep.running      = true;
    s_mode = DDS_MODE_SWEEP;
    return true;
}

void dds_sweep_start(void) { UART_Printf("[dds] sweep_start\r\n"); s_sweep.running = true;  ad9910_io_update(); }
void dds_sweep_stop (void) { UART_Printf("[dds] sweep_stop\r\n");  s_sweep.running = false; ad9910_drg_disable(); s_mode = DDS_MODE_TONE; }

/* ==================================================================
 *  §5  Mode 4 · 快速跳频 (Multi-Profile)
 * ================================================================== */

bool dds_hop_load(const double *freq_list, const float *amp_list, uint8_t n)
{
    UART_Printf("[dds] hop_load: n=%u profile(s)\r\n", (unsigned)n);
    if (!freq_list || n == 0 || n > 8) return false;

    enter_single_tone_mode();          /* 关 RAM/DRG，只留 profile */
    for (uint8_t i = 0; i < n; ++i) {
        float amp_v = amp_list ? amp_list[i] : DDS_FULL_SCALE_V;
        s_hop.freq_list[i] = freq_list[i];
        s_hop.amp_list [i] = amp_v;
        UART_Printf("[dds]   profile[%u]: f=%.3f Hz  amp=%.3f V\r\n",
                    (unsigned)i, freq_list[i], (double)amp_v);
        ad9910_profile_write(i,
            ad9910_amp01_to_asf(amp_v_to_amp01(amp_v)),
            0,
            ad9910_freq_to_ftw (freq_list[i]));
    }
    s_hop.n         = n;
    s_hop.idx       = 0;
    s_hop.dir       = +1;
    s_hop.mode      = DDS_HOP_MODE_MANUAL;
    s_hop.period_ms = 0;
    s_hop.last_ms   = HAL_GetTick();
    ad9910_profile_select(0);
    s_mode = DDS_MODE_HOP;
    return true;
}

void dds_hop_select(uint8_t idx)
{
    if (idx >= s_hop.n) return;
    UART_Printf("[dds] hop_select: idx=%u  f=%.3f Hz\r\n",
                (unsigned)idx, s_hop.freq_list[idx]);
    s_hop.idx = idx;
    ad9910_profile_select(idx);
}

void dds_hop_task_config(uint32_t period_ms, dds_hop_mode_t mode)
{
    UART_Printf("[dds] hop_task_config: period=%lu ms  mode=%d\r\n",
                (unsigned long)period_ms, (int)mode);
    s_hop.mode      = mode;
    s_hop.period_ms = period_ms;
    s_hop.last_ms   = HAL_GetTick();
}

void dds_hop_stop(void)
{
    UART_Printf("[dds] hop_stop\r\n");
    s_hop.mode = DDS_HOP_MODE_MANUAL;
    s_hop.period_ms = 0;
    /* 停留在最后一个 profile 上；关输出请调 dds_stop() */
}

/* ==================================================================
 *  §6  Mode 5 · OSK
 * ================================================================== */

void dds_osk_begin(double freq_hz, float amp_v)
{
    UART_Printf("[dds] osk_begin: carrier f=%.3f Hz  amp=%.3f V\r\n",
                freq_hz, (double)amp_v);
    enter_single_tone_mode();
    ad9910_set_tone(&(ad9910_tone_t){
        .f_hz = freq_hz, .amp01 = amp_v_to_amp01(amp_v), .phase_deg = 0.0f });
    ad9910_osk_enable(true);
    ad9910_osk_key(false);           /* 默认关闭 */
    s_mode = DDS_MODE_OSK;
}

void dds_osk_key(bool on)  { UART_Printf("[dds] osk_key: %s\r\n", on ? "ON" : "OFF"); ad9910_osk_key(on); }

void dds_osk_end(void)
{
    UART_Printf("[dds] osk_end (back to single-tone)\r\n");
    ad9910_osk_key(false);
    ad9910_osk_enable(false);
    s_mode = DDS_MODE_TONE;
}

/* ==================================================================
 *  §7  任务
 *
 *  只在 HOP 自动模式里干活；SWEEP / OSK / TONE / ARB 都是硬件持续运行，不需要 CPU。
 * ================================================================== */

void dds_task(void)
{
    if (s_mode != DDS_MODE_HOP) return;
    if (s_hop.mode == DDS_HOP_MODE_MANUAL) return;
    if (s_hop.period_ms == 0) return;

    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - s_hop.last_ms) < s_hop.period_ms) return;
    s_hop.last_ms = now;

    /* 计算下一个 idx */
    int8_t next = (int8_t)s_hop.idx;
    if (s_hop.mode == DDS_HOP_MODE_CYCLE) {
        next = (int8_t)((s_hop.idx + 1) % s_hop.n);
    } else if (s_hop.mode == DDS_HOP_MODE_PING_PONG) {
        next = (int8_t)s_hop.idx + s_hop.dir;
        if (next >= (int8_t)s_hop.n) { next = (int8_t)s_hop.n - 2; s_hop.dir = -1; }
        if (next < 0)                { next = 1;                    s_hop.dir = +1; }
        if (next < 0) next = 0;
    }
    s_hop.idx = (uint8_t)next;
    ad9910_profile_select(s_hop.idx);
}

/* ==================================================================
 *  §8  通用 init / stop / dump
 * ================================================================== */

void dds_init(void)
{
    UART_Printf("[dds] init begin (full-scale calibration = %.3f V)\r\n",
                (double)DDS_FULL_SCALE_V);
    memset(&s_sweep, 0, sizeof(s_sweep));
    memset(&s_hop,   0, sizeof(s_hop));

    ad9910_init();
    /* 上电即"待命":单音模式、输出关闭。业务侧再显式设频/幅/相 */
    s_mode = DDS_MODE_TONE;
    UART_Printf("[dds] init done, mode=TONE (muted)\r\n");
}

void dds_stop(void)
{
    UART_Printf("[dds] stop (mute output, keep mode)\r\n");
    ad9910_output_enable(false);
}

dds_mode_t dds_get_mode(void) { return s_mode; }

void dds_dump(void)
{
    const char *name = "?";
    switch (s_mode) {
    case DDS_MODE_IDLE:  name = "IDLE";  break;
    case DDS_MODE_TONE:  name = "TONE";  break;
    case DDS_MODE_ARB:   name = "ARB";   break;
    case DDS_MODE_SWEEP: name = "SWEEP"; break;
    case DDS_MODE_HOP:   name = "HOP";   break;
    case DDS_MODE_OSK:   name = "OSK";   break;
    }
    UART_Printf("[dds] mode = %s\r\n", name);

    if (s_mode == DDS_MODE_SWEEP && s_sweep.running) {
        UART_Printf("  sweep dest=%u style=%u T=%.3fs\r\n",
                    (unsigned)s_sweep.dest, (unsigned)s_sweep.style,
                    (double)s_sweep.sweep_time_s);
    } else if (s_mode == DDS_MODE_HOP) {
        UART_Printf("  hop n=%u idx=%u mode=%u period=%lu ms\r\n",
                    (unsigned)s_hop.n, (unsigned)s_hop.idx,
                    (unsigned)s_hop.mode, (unsigned long)s_hop.period_ms);
    }
    ad9910_dump_state();
}
