#ifndef __MODULE_DDS_H
#define __MODULE_DDS_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  DDS 输出模块 (module 层)
 * ===========================================================================
 *
 *  职责：
 *    - 把 AD9910 芯片的 5 种输出能力，包装成"业务能理解"的 API
 *    - 幅度换算：用户传 V，模块把它换算成 ASF (依赖 DDS_FULL_SCALE_V 校准)
 *    - 内嵌任意波形样点生成器 (sine/square/triangle/sawtooth/callback)
 *    - 长时序模式 (扫频/跳频/OSK 键控) 的状态机由 dds_task() 推进
 *
 *  与 drv/ad9910 的分工：
 *    - drv：寄存器、SPI、GPIO；不知道"扫频""任意波"是什么
 *    - dds：只写业务逻辑，寄存器读写全部委托 drv
 *
 *  五种模式：
 *    Mode 1  单音        (Single-Tone)      —— 用 AD9910 原生 profile 输出正弦
 *    Mode 2  任意波形    (RAM playback)     —— 从 RAM 播放 1024 点样点
 *    Mode 3  扫频        (DRG linear ramp)  —— 线性扫 F / A / P
 *    Mode 4  快速跳频    (Multi-profile)    —— 8 个 profile + 引脚切换
 *    Mode 5  OSK 键控    (Output Shift Key) —— ASK/OOK
 *
 *  典型用法：
 *      dds_init();
 *      // Mode 1: 单音
 *      dds_tone_sine(1000.0, 1.0f, 0.0f);
 *      // Mode 2: 任意波 (500Hz 50% 方波, 0.8V)
 *      dds_arb_square(500.0, 0.5f, 0.8f);
 *      // Mode 3: 100Hz-100kHz 频率线性扫描, 1 秒扫完, 单程
 *      dds_sweep_freq(100.0, 100000.0, 1.0f, DDS_SWEEP_ONESHOT);
 *      dds_sweep_start();
 *      // Mode 4: 3 个频点循环跳
 *      double f[3] = {1e3, 1e4, 1e5};
 *      dds_hop_load(f, NULL, 3);
 *      dds_hop_task_config(100, DDS_HOP_MODE_CYCLE);
 *      // Mode 5: OSK 键控
 *      dds_osk_begin(1e6, 1.0f);
 *      dds_osk_key(true);
 *      dds_osk_key(false);
 * ===========================================================================
 */

/* ---- 全量程电压 (板子实测校准)。默认 1.0V，用户根据 balun/Rset 修改。---- */
#ifndef DDS_FULL_SCALE_V
#define DDS_FULL_SCALE_V         1.0f
#endif

/* ---- 任意波形 RAM 深度 ---- */
#define DDS_ARB_RAM_DEPTH        1024U

/* ==========================================================================
 *  当前模式
 * ========================================================================== */
typedef enum {
    DDS_MODE_IDLE = 0,     /* 未初始化或已 disable */
    DDS_MODE_TONE,         /* Mode 1 单音 */
    DDS_MODE_ARB,          /* Mode 2 RAM 任意波 */
    DDS_MODE_SWEEP,        /* Mode 3 DRG 扫频 */
    DDS_MODE_HOP,          /* Mode 4 profile 跳频 */
    DDS_MODE_OSK,          /* Mode 5 OSK 键控 */
} dds_mode_t;

/* ==========================================================================
 *  生命周期 + 通用
 * ========================================================================== */
void       dds_init(void);           /* 内部会 ad9910_init() */
void       dds_task(void);           /* 注册到 scheduler；驱动扫频/跳频状态机 */
void       dds_stop(void);           /* 关输出 (ASF=0)，模式保持 */
dds_mode_t dds_get_mode(void);

/* ==========================================================================
 *  Mode 1 · 单音 (Single-Tone Profile)
 *  -------------------------------------
 *  最"干净"的正弦输出。amp_v 单位 V，内部换算成 ASF；超过 DDS_FULL_SCALE_V
 *  会硬饱和到最大 (ASF=0x3FFF)。
 * ========================================================================== */
void dds_tone_sine (double freq_hz, float amp_v, float phase_deg);
void dds_tone_freq (double freq_hz);          /* 只改频率 */
void dds_tone_amp  (float  amp_v);            /* 只改幅度 */
void dds_tone_phase(float  phase_deg);        /* 只改相位 */

/* ==========================================================================
 *  Mode 2 · 任意波形 (RAM playback)
 *  --------------------------------
 *  播放速率 = SYSCLK / 4 / (rate_divider+1) 个 RAM 步/秒。
 *  → 完整一个周期 = N 个样点，波形频率 = 播放速率 / N
 *
 *  用户不用管这个换算，下面 API 直接接受 freq_hz，内部计算 rate_divider。
 *  能覆盖的频率范围：约 1 Hz ~ 250 kHz (受 RAM 深度和步进速率上限限制)
 * ========================================================================== */

/* 直接给样点。samples[i] 是 -1.0f ~ +1.0f 归一后的值。 */
bool dds_arb_load(const float *samples, uint16_t n, double freq_hz, float amp_v);

/* 用户函数生成样点：f(t_norm) → -1..1，t_norm ∈ [0,1)。 */
typedef float (*dds_arb_fn_t)(float t_norm);
bool dds_arb_from_fn(dds_arb_fn_t fn, uint16_t n, double freq_hz, float amp_v);

/* 常用波形一键出。走 RAM 通道，方便对比与调试。 */
bool dds_arb_sine    (double freq_hz, float amp_v);
bool dds_arb_square  (double freq_hz, float duty, float amp_v);    /* duty 0..1 */
bool dds_arb_triangle(double freq_hz, float amp_v);
bool dds_arb_sawtooth(double freq_hz, float amp_v);

/* 调试用：RAM 全填相同值 → 输出应该是稳定直流 (值 = amp_v)。
 *   OUT+ 有稳定电压 → RAM 使能/写入/播放机制 OK, 只是波形算法错
 *   OUT+ 无信号     → RAM 通路根本不通, 需要查 CFR1/profile 位定义 */
bool dds_arb_dc      (float amp_v);

/* ==========================================================================
 *  Mode 3 · 扫频 (DRG)
 *  --------------------
 *  硬件线性扫描，扫完可选停留、回扫或循环。sweep_time_s 是走完一斜坡的时长。
 * ========================================================================== */
typedef enum {
    DDS_SWEEP_ONESHOT = 0,     /* 走一遍就停在终点 */
    DDS_SWEEP_BIDIR   = 1,     /* 上-下往返 */
    DDS_SWEEP_CONT_UP = 2,     /* 到顶立即从底重来 (锯齿) */
} dds_sweep_style_t;

/* 频率扫描 */
bool dds_sweep_freq(double f_start_hz, double f_stop_hz,
                    float sweep_time_s, dds_sweep_style_t style);
/* 幅度扫描 (amp_v_start → amp_v_stop) */
bool dds_sweep_amp (float v_start, float v_stop,
                    float sweep_time_s, dds_sweep_style_t style);
/* 相位扫描 (0..360) */
bool dds_sweep_phase(float deg_start, float deg_stop,
                     float sweep_time_s, dds_sweep_style_t style);

void dds_sweep_start(void);
void dds_sweep_stop (void);

/* ==========================================================================
 *  Mode 4 · 快速跳频 (Multi-Profile Hop)
 *  -------------------------------------
 *  预写 N 个 profile (N ≤ 8)，然后：
 *    - 手动切: dds_hop_select(idx)
 *    - 自动切: dds_hop_task_config(period_ms, DDS_HOP_MODE_XXX) + 让 dds_task 驱动
 * ========================================================================== */
typedef enum {
    DDS_HOP_MODE_MANUAL = 0,    /* 只手动切 */
    DDS_HOP_MODE_CYCLE  = 1,    /* 0→1→…→N-1→0 循环 */
    DDS_HOP_MODE_PING_PONG = 2, /* 0→1→…→N-1→N-2→…→0 */
} dds_hop_mode_t;

/* freq_list[n]：频率列表; amp_list 可传 NULL (全用 DDS_FULL_SCALE_V) */
bool dds_hop_load(const double *freq_list, const float *amp_list, uint8_t n);
void dds_hop_select(uint8_t idx);
void dds_hop_task_config(uint32_t period_ms, dds_hop_mode_t mode);
void dds_hop_stop(void);

/* ==========================================================================
 *  Mode 5 · OSK 键控 (ASK / OOK)
 *  ------------------------------
 *  基波频率/幅度用当前 profile 0 的设置。dds_osk_key() 通过 OSK 引脚开关输出。
 * ========================================================================== */
void dds_osk_begin(double freq_hz, float amp_v);   /* 设置载波并进入 OSK 模式 */
void dds_osk_key  (bool on);                       /* true = 输出打开 */
void dds_osk_end  (void);                          /* 回单音模式 */

/* ==========================================================================
 *  调试
 * ========================================================================== */
void dds_dump(void);   /* 打印当前模式 + 参数 */

#endif /* __MODULE_DDS_H */
