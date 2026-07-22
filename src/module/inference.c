/*
 * inference.c —— 未知电路推理输出 (发挥部分 (2))
 *
 * 用 learning 得到的 (ω0, ζ, K, type) 构造二阶数字 IIR (biquad),
 * ADC 采输入信号 → biquad 差分方程实时运算 → DAC 输出.
 *
 *      信号发生器 → 装置输入口 → ADC → biquad → DAC → 装置输出口 → 示波器
 *
 * 数学:
 *   连续域 (report 2.3 节):
 *       LPF:  H(s) = K·ω0² / (s² + 2ζω0·s + ω0²)
 *       HPF:  H(s) = K·s²   / (s² + 2ζω0·s + ω0²)
 *       BPF:  H(s) = K·(2ζω0·s) / (s² + 2ζω0·s + ω0²)
 *       BSF:  H(s) = K·(s² + ω0²) / (s² + 2ζω0·s + ω0²)
 *   双线性变换 s = (2/T)·(1 - z^-1)/(1 + z^-1) 得到:
 *       H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
 *
 * 说明:
 *   本文件包含 IIR biquad 的完整实现与 ADC 通路接入.
 *   DAC 输出接口预留 hook (dac_out_write), 硬件到位后填 STM32 DAC
 *   单样点写即可; 目前未开 DAC 时, IIR 结果打到 UART 供调试.
 */

#include "module/inference.h"
#include "module/learning.h"
#include "bsp/adc.h"
#include "bsp/uart.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

/* ==================================================================
 *  参数
 * ================================================================== */
#define INFER_ADC_FS_HZ    200000UL       /* 200 kSPS, 满足 50 kHz Nyquist */
#define INFER_ADC_CH       ADC_CHANNEL_1
#define INFER_ADC_BUF_LEN  256U           /* 每次半满 128 样点 */
#define INFER_ADC_DC       2048           /* 12-bit 中值 */
#define INFER_DAC_DC       2048

/* ==================================================================
 *  Biquad Direct Form I
 *
 *   y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2]
 *        - a1·y[n-1] - a2·y[n-2]
 * ================================================================== */
typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float x1, x2;
    float y1, y2;
} biquad_t;

static void biquad_reset(biquad_t *b)
{
    b->x1 = b->x2 = b->y1 = b->y2 = 0.0f;
}

static inline float biquad_step(biquad_t *b, float x)
{
    float y = b->b0 * x
            + b->b1 * b->x1
            + b->b2 * b->x2
            - b->a1 * b->y1
            - b->a2 * b->y2;
    b->x2 = b->x1;  b->x1 = x;
    b->y2 = b->y1;  b->y1 = y;
    return y;
}

/* ==================================================================
 *  双线性变换: 通用 2 阶 s 域 → z 域
 *
 *  输入连续域分子分母:
 *     H(s) = (Bs2·s² + Bs1·s + Bs0) / (As2·s² + As1·s + As0)
 *  代入 s = c·(1 - z^-1)/(1 + z^-1), c = 2·fs, 分子分母各乘 (1+z^-1)²:
 *     num(z) = (Bs2·c² + Bs1·c + Bs0)
 *            + (-2·Bs2·c² + 2·Bs0)·z^-1
 *            + (Bs2·c² - Bs1·c + Bs0)·z^-2
 *  den(z) 同理. 整体除 den[0] 归一化.
 * ================================================================== */
static void bilinear(float Bs2, float Bs1, float Bs0,
                     float As2, float As1, float As0,
                     float fs, biquad_t *b)
{
    float c  = 2.0f * fs;
    float c2 = c * c;

    float num0 = Bs2 * c2 + Bs1 * c + Bs0;
    float num1 = -2.0f * Bs2 * c2 + 2.0f * Bs0;
    float num2 = Bs2 * c2 - Bs1 * c + Bs0;

    float den0 = As2 * c2 + As1 * c + As0;
    float den1 = -2.0f * As2 * c2 + 2.0f * As0;
    float den2 = As2 * c2 - As1 * c + As0;

    if (fabsf(den0) < 1e-20f) den0 = 1e-20f;

    b->b0 = num0 / den0;
    b->b1 = num1 / den0;
    b->b2 = num2 / den0;
    b->a1 = den1 / den0;
    b->a2 = den2 / den0;
}

/* ==================================================================
 *  学习结果 → biquad
 * ================================================================== */
static void model_to_biquad(const learn_result_t *r, float fs, biquad_t *b)
{
    float w0   = r->params[0];
    float zeta = r->params[1];
    float K    = r->params[2];

    /* 分母对四种滤波器都一样: s² + 2ζω0·s + ω0² */
    float As2 = 1.0f;
    float As1 = 2.0f * zeta * w0;
    float As0 = w0 * w0;

    /* 分子: 见文件头注释 */
    float Bs2 = 0.0f, Bs1 = 0.0f, Bs0 = 0.0f;
    switch (r->type) {
    case FILTER_LOWPASS:
        Bs0 = K * w0 * w0;
        break;
    case FILTER_HIGHPASS:
        Bs2 = K;
        break;
    case FILTER_BANDPASS:
        Bs1 = K * 2.0f * zeta * w0;
        break;
    case FILTER_BANDSTOP:
        Bs2 = K;
        Bs0 = K * w0 * w0;
        break;
    default:
        Bs0 = K;   /* 直通 */
        break;
    }

    bilinear(Bs2, Bs1, Bs0, As2, As1, As0, fs, b);
    biquad_reset(b);
}

/* ==================================================================
 *  DAC 单样点输出 (STM32F103 DAC1, PA4).
 *
 *  当前实现: 直接写 DAC->DHR12R1 数据寄存器.
 *  依赖:
 *      SystemClock + GPIOA 时钟已开
 *      PA4 已配为模拟输入 (analog), 由 GPIO 初始化处理
 *      DAC 时钟已在 dac_out_init 里使能
 * ================================================================== */
#include "stm32f1xx_hal.h"

static bool s_dac_ready = false;

static void dac_out_init(void)
{
    if (s_dac_ready) return;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DAC_CLK_ENABLE();

    /* PA4 = DAC1_OUT1: analog mode */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* 使能 DAC1 CH1, 软件触发 */
    DAC->CR &= ~(DAC_CR_EN1 | DAC_CR_TSEL1 | DAC_CR_TEN1 | DAC_CR_BOFF1);
    DAC->CR |=  DAC_CR_EN1;
    DAC->DHR12R1 = INFER_DAC_DC;

    s_dac_ready = true;
}

static inline void dac_out_write(uint16_t v12)
{
    if (v12 > 4095) v12 = 4095;
    DAC->DHR12R1 = v12;
}

/* ==================================================================
 *  推理运行时状态
 * ================================================================== */
static inference_state_t s_state = INFER_IDLE;
static biquad_t          s_biq;
static uint16_t          s_adc_buf[INFER_ADC_BUF_LEN];

/* ==================================================================
 *  ADC 半满/全满回调 —— 块处理 IIR + DAC 输出
 *
 *  说明:
 *    ADC1 用 TIM3 触发 + DMA 循环模式, 半满/全满各触发一次回调,
 *    每次带 INFER_ADC_BUF_LEN / 2 个新样点. 本回调内一次跑完所有
 *    新样点的 biquad, 每样点后立即写 DAC.
 *    延迟 = 半 buffer 时间 = 128/200k = 640 us, 已够 "同频稳定" 要求
 *    (相位固定, 无频飘). 需要更低延迟可改成 ADC EOC 中断单点处理.
 * ================================================================== */
static void on_adc_block(const uint16_t *raw, uint16_t n)
{
    for (uint16_t i = 0; i < n; ++i) {
        float x = (float)((int32_t)raw[i] - INFER_ADC_DC);
        float y = biquad_step(&s_biq, x);

        int32_t out = (int32_t)(y + (float)INFER_DAC_DC + 0.5f);
        if (out < 0)    out = 0;
        if (out > 4095) out = 4095;
        dac_out_write((uint16_t)out);
    }
}

/* ==================================================================
 *  生命周期 API
 * ================================================================== */
void inference_init(void)
{
    s_state = INFER_IDLE;
    memset(&s_biq, 0, sizeof(s_biq));
    dac_out_init();
}

void inference_task(void)
{
    /* IIR 全部在 ADC 回调里跑, task 只做偶发检查 */
}

bool inference_start(void)
{
    if (s_state == INFER_RUNNING) return false;

    const learn_result_t *r = learning_get_result();
    if (learning_get_state() != LEARN_DONE || r->type == FILTER_UNKNOWN) {
        UART_Printf("[infer] cannot start: no learned model\r\n");
        s_state = INFER_FAILED;
        return false;
    }

    model_to_biquad(r, (float)INFER_ADC_FS_HZ, &s_biq);
    UART_Printf("[infer] biquad: b=(%.4f %.4f %.4f) a1=%.4f a2=%.4f\r\n",
                (double)s_biq.b0, (double)s_biq.b1, (double)s_biq.b2,
                (double)s_biq.a1, (double)s_biq.a2);

    dac_out_init();
    ADC_SetFFTCallback(on_adc_block);
    if (!ADC_StartFFT(INFER_ADC_CH, INFER_ADC_FS_HZ,
                      s_adc_buf, INFER_ADC_BUF_LEN)) {
        UART_Printf("[infer] ADC start failed\r\n");
        s_state = INFER_FAILED;
        return false;
    }

    s_state = INFER_RUNNING;
    UART_Printf("[infer] RUNNING @ fs=%lu Hz\r\n",
                (unsigned long)INFER_ADC_FS_HZ);
    return true;
}

void inference_stop(void)
{
    ADC_StopFFT();
    dac_out_write(INFER_DAC_DC);
    s_state = INFER_IDLE;
    UART_Printf("[infer] stopped\r\n");
}

inference_state_t inference_get_state(void)             { return s_state; }
void              inference_set_state(inference_state_t s) { s_state = s; }
