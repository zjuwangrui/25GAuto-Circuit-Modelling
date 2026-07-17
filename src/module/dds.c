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
    enter_single_tone_mode();
    ad9910_tone_t t = {
        .f_hz = f_hz, .amp01 = amp_v_to_amp01(amp_v), .phase_deg = phase_deg,
    };
    ad9910_set_tone(&t);
}

void dds_tone_freq (double f_hz)   { enter_single_tone_mode(); ad9910_set_freq_hz(f_hz); }
void dds_tone_amp  (float  amp_v)  { enter_single_tone_mode(); ad9910_set_amp01(amp_v_to_amp01(amp_v)); }
void dds_tone_phase(float  deg)    { enter_single_tone_mode(); ad9910_set_phase_deg(deg); }

/* ==================================================================
 *  §3  Mode 2 · 任意波 (RAM playback, ASF 目的模式)
 *
 *  与实测例子 (resources/1.AD9910模块-三角波点频输出代码) 对齐:
 *    - RAM 目的 = ASF (不用 Polar), DAC 输出直接正比 ASF
 *    - RAM 每字 32-bit, 只有 [31:18] 存 14-bit ASF, 其余 0
 *    - RAM 数据是"无符号幅度包络" (0 = 最低电压 ~250mV DC, 16383 = 满量程 500mV)
 *    - FTW = 0 → DDS 载波不动, DAC 输出跟随 ASF (t) 直接给出波形形状
 *
 *  波形样点编码约定 (供业务侧调用):
 *    - 用户给的 sample ∈ [-1, +1] 对称范围 (数学直觉)
 *    - 内部映射到 unipolar ASF: mag = 0.5 + 0.5 * (sample * amp01)
 *    - 结果: sample=-1 → ASF=0 → 最低 DAC 输出; sample=+1 → ASF=full → 最高
 *    - 输出峰峰值 (单端) = amp01 * 500 mV  (受 AD9910 单极性 DAC 限制)
 *
 *  播放速率 (与例子对齐, 无 -1 偏移):
 *    freq = SYSCLK / (4 * M) / N
 *    → M = SYSCLK / (4 * N * freq)      (M 范围 1..65535)
 *    1 kHz / N=1024: M = 1e9 / (4*1024*1000) = 244
 * ================================================================== */

static uint32_t s_arb_ram[DDS_ARB_RAM_DEPTH];   /* 组包缓冲 */

/* 把 -1..+1 的 float 样点编成 RAM 字 (ASF-only 模式).
 * 通过 mag = 0.5 + 0.5*s*amp01 把双极性映射到单极性 [0..1],
 * 再放大到 14-bit ASF (0..16383), 存到 [31:18].
 * DAC 输出 = ASF * IOUT_FS, 通过 50Ω 得到 0..500mV 的电压. */
static uint32_t sample_to_ram_word(float s, float amp01)
{
    float mag = 0.5f + 0.5f * s * amp01;
    if (mag < 0.0f) mag = 0.0f;
    if (mag > 1.0f) mag = 1.0f;
    uint16_t asf14 = (uint16_t)(mag * 16383.0f + 0.5f);
    return ((uint32_t)(asf14 & 0x3FFF) << 18);
}

static bool arb_load_ram_and_start(uint16_t n, double freq_hz, float amp01)
{
    if (n == 0 || n > DDS_ARB_RAM_DEPTH) return false;
    if (freq_hz <= 0.0) return false;

    /* --- Rate M 计算 (与例子完全对齐, 无 -1) --- */
    double step_clk = (double)AD9910_SYSCLK_HZ / 4.0;   /* 250 MHz */
    double m_d      = step_clk / ((double)n * freq_hz);
    if (m_d < 1.0)      m_d = 1.0;
    if (m_d > 65535.0)  m_d = 65535.0;
    uint16_t rate_m = (uint16_t)(m_d + 0.5);

    /* --- 切模式 --- */
    ad9910_drg_disable();
    ad9910_profile_select(0);

    /* --- 写 RAM 数据 (会顺便设 profile 起始地址) --- */
    ad9910_ram_write(0, s_arb_ram, n);

    /* --- 覆写 profile 0 = 真正的 RAM playback 参数 --- */
    ad9910_ram_profile_t cfg = {
        .start_addr    = 0,
        .end_addr      = (uint16_t)(n - 1),
        .rate_divider  = rate_m,
        /* 与例子对齐: 用 mode = 4. 现测得 mode 3 芯片行为是"连续双向"
         * (每 2048 样点一个循环, 非回文数据的波形频率会减半),
         * mode 4 才是真正的"连续单向环回", 保证方波频率跟设置一致. */
        .mode          = AD9910_RAM_MODE_CONTINUOUS_BIDIR,   /* = 4 */
        .no_dwell_high = false,
        .zero_crossing = false,
    };
    ad9910_ram_profile_config(0, &cfg);

    /* --- 使能 RAM, 目的 = ASF (与例子对齐) --- */
    ad9910_ram_enable(AD9910_CFR1_RAM_DEST_ASF);
    s_mode = DDS_MODE_ARB;
    (void)amp01;    /* amp01 已经在样点里体现了 */
    return true;
}

bool dds_arb_load(const float *samples, uint16_t n, double freq_hz, float amp_v)
{
    if (!samples) return false;
    float amp01 = amp_v_to_amp01(amp_v);
    for (uint16_t i = 0; i < n; ++i) s_arb_ram[i] = sample_to_ram_word(samples[i], amp01);
    return arb_load_ram_and_start(n, freq_hz, amp01);
}

bool dds_arb_from_fn(dds_arb_fn_t fn, uint16_t n, double freq_hz, float amp_v)
{
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

/* --- 调试用: RAM 全填相同值 → OUT+ 应该输出稳定 DC ---
 *   如果示波器上看到稳定电压 (幅度 ~amp_v) → RAM 通路 + Polar 编码 mag 位分配 OK
 *   如果 OUT+ 什么都没有 / 是噪声 → RAM 使能位 / profile 位定义有 bug
 *
 *  实现: 把 1024 个 RAM 字都填成 sample=+1.0 * amp01 (mag=amp01, POW=0°)
 *        rate_divider 任意选一个 (取 1kHz 对应值), 因为所有样点相同, 输出恒定
 */
bool dds_arb_dc(float amp_v)
{
    float amp01 = amp_v_to_amp01(amp_v);
    uint32_t ram_word = sample_to_ram_word(1.0f, amp01);   /* 全正, POW=0° */
    for (uint16_t i = 0; i < DDS_ARB_RAM_DEPTH; ++i) s_arb_ram[i] = ram_word;
    return arb_load_ram_and_start(DDS_ARB_RAM_DEPTH, 1000.0, amp01);
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

void dds_sweep_start(void) { s_sweep.running = true;  ad9910_io_update(); }
void dds_sweep_stop (void) { s_sweep.running = false; ad9910_drg_disable(); s_mode = DDS_MODE_TONE; }

/* ==================================================================
 *  §5  Mode 4 · 快速跳频 (Multi-Profile)
 * ================================================================== */

bool dds_hop_load(const double *freq_list, const float *amp_list, uint8_t n)
{
    if (!freq_list || n == 0 || n > 8) return false;

    enter_single_tone_mode();          /* 关 RAM/DRG，只留 profile */
    for (uint8_t i = 0; i < n; ++i) {
        float amp_v = amp_list ? amp_list[i] : DDS_FULL_SCALE_V;
        s_hop.freq_list[i] = freq_list[i];
        s_hop.amp_list [i] = amp_v;
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
    s_hop.idx = idx;
    ad9910_profile_select(idx);
}

void dds_hop_task_config(uint32_t period_ms, dds_hop_mode_t mode)
{
    s_hop.mode      = mode;
    s_hop.period_ms = period_ms;
    s_hop.last_ms   = HAL_GetTick();
}

void dds_hop_stop(void)
{
    s_hop.mode = DDS_HOP_MODE_MANUAL;
    s_hop.period_ms = 0;
    /* 停留在最后一个 profile 上；关输出请调 dds_stop() */
}

/* ==================================================================
 *  §6  Mode 5 · OSK
 * ================================================================== */

void dds_osk_begin(double freq_hz, float amp_v)
{
    enter_single_tone_mode();
    ad9910_set_tone(&(ad9910_tone_t){
        .f_hz = freq_hz, .amp01 = amp_v_to_amp01(amp_v), .phase_deg = 0.0f });
    ad9910_osk_enable(true);
    ad9910_osk_key(false);           /* 默认关闭 */
    s_mode = DDS_MODE_OSK;
}

void dds_osk_key(bool on)  { ad9910_osk_key(on); }

void dds_osk_end(void)
{
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
    memset(&s_sweep, 0, sizeof(s_sweep));
    memset(&s_hop,   0, sizeof(s_hop));

    ad9910_init();
    /* 上电即"待命":单音模式、输出关闭。业务侧再显式设频/幅/相 */
    s_mode = DDS_MODE_TONE;
}

void dds_stop(void)
{
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
