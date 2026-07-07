#include "module/ui.h"
#include "module/thd.h"
#include "module/terminal.h"
#include "bsp/lcd.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <math.h>

#define UI_MAX_PAGES  4

static const ui_page_t *s_pages[UI_MAX_PAGES];
static uint8_t          s_page_count = 0;
static const ui_page_t *s_current    = 0;
static bool             s_dirty      = true;   /* 需要 enter() 一次 */

/* ============================================================
 *  Page 1: 欢迎页
 * ============================================================ */

static void welcome_enter(void)
{
    LCD_Clear(BLACK);
    LCD_DrawTextC( 40,  30, YELLOW, BLACK, "STM32 App Framework");
    LCD_DrawTextC( 40,  55, WHITE,  BLACK, "----------------------------");
    LCD_DrawTextC( 40,  80, WHITE,  BLACK, "core   : scheduler + ring");
    LCD_DrawTextC( 40,  98, WHITE,  BLACK, "module : terminal + thd + ui");
    LCD_DrawTextC( 40, 116, WHITE,  BLACK, "bsp    : uart + adc + lcd + led");
    LCD_DrawTextC( 40, 150, GREEN,  BLACK, "Type 'help' on USART1 to start");
}

static void welcome_tick(void)
{
    uint32_t s = HAL_GetTick() / 1000U;
    LCD_DrawTextf(40, 200, WHITE, BLACK, "uptime: %lu s   ", (unsigned long)s);
}

static const ui_page_t s_welcome = {
    .name  = "welcome",
    .enter = welcome_enter,
    .tick  = welcome_tick,
};

/* ============================================================
 *  Page 2: THD 显示
 *
 *  布局（320×240 横屏）：
 *   y=  0   Title "THD Meter (Goertzel)"                  16 px
 *   y= 20   ┌ waveform 320×50 (5 次谐波重建) ┐            50 px
 *   y= 80   f0 值 + THD 值                                  16 px
 *   y=100   H1 bar + 数值                                   16 px
 *   y=120   H2                                              16 px
 *   y=140   H3
 *   y=160   H4
 *   y=180   H5
 *   y=210   Frame # / Fs / RUN|STOP                         16 px
 * ============================================================ */

/* --- 波形显示参数 --- */
#define WAVE_X                0
#define WAVE_Y                20
#define WAVE_W              320       /* 全屏宽 */
#define WAVE_H               50
#define WAVE_CENTER_Y       (WAVE_Y + WAVE_H / 2)
#define WAVE_PIX_PER_PERIOD  160      /* 屏上显示 2 个周期 (320/160=2) */
#define WAVE_HALF_H         (WAVE_H / 2 - 2)     /* 波形垂直半幅像素 */

/* --- 数值/条形参数 --- */
#define THD_BAR_X       40
#define THD_BAR_MAX_W  180
#define THD_BAR_H       12
#define THD_VAL_X      230
#define THD_H1_Y       100
#define THD_ROW_DY      20

/* 每个谐波上一次画的条宽，用于差量重画 */
static int      s_last_bar_w[THD_MAX_HARMONIC] = {0};

/* 上一次绘制波形对应的 frame_id；相同则跳过重画避免闪烁 */
static uint32_t s_last_wave_frame = 0xFFFFFFFFU;

/* ------------------------------------------------------------
 *  波形绘制：读取 H[k] 幅度和相位，用 y = ΣA_k cos(2πk·t + φ_k)
 *  在 320 列上算 320 个点然后连线。
 *
 *  技巧：
 *   1) 时间轴用"周期数"归一：t_norm = x / WAVE_PIX_PER_PERIOD
 *      则 cos(2πk·f0·t + φ) = cos(2π·k·t_norm + φ)，f0 自动消掉。
 *   2) 用 H1 的相位 φ_1 做时间参考，画出来的波形相对稳定
 *      （不会因为帧起点漂移而抖动）：
 *          effective φ_k = φ_k - k·φ_1
 *   3) 归一化：估计峰峰值 ≈ Σ|A_k|，把波形缩放到 ±WAVE_HALF_H 像素内。
 * ------------------------------------------------------------ */
static void wave_redraw(const thd_result_t *r)
{
    /* 先清空波形区域 */
    LCD_FillRect(WAVE_X, WAVE_Y, WAVE_W, WAVE_H, BLACK);

    /* 中线（浅蓝，作为零轴参考） */
    LCD_FillRect(WAVE_X, WAVE_CENTER_Y, WAVE_W, 1, BLUE);

    /* 峰峰估计：所有谐波幅度之和（放大保守，避免溢出） */
    float peak_est = 0.0f;
    for (int k = 0; k < THD_MAX_HARMONIC; ++k) peak_est += r->harmonic[k];
    if (peak_est < 1e-3f) return;                /* 无信号，只留中线 */

    /* 相对于 H1 相位的相移，让波形显示"触发"到基频零相位 */
    float phi1 = r->harmonic_phase[0];
    float eff_phi[THD_MAX_HARMONIC];
    for (int k = 0; k < THD_MAX_HARMONIC; ++k)
        eff_phi[k] = r->harmonic_phase[k] - (float)(k + 1) * phi1;

    /* 每像素上的角度增量：对第 k+1 次谐波
     *   Δω_k = 2π · (k+1) / WAVE_PIX_PER_PERIOD */
    const float TWO_PI = 6.28318530717958647692f;
    float scale_y = (float)WAVE_HALF_H / peak_est;

    /* 用绿色连线 */
    LCD_SetColors(GREEN, BLACK);

    int prev_y = -1;
    for (int x = 0; x < WAVE_W; ++x) {
        float t_norm = (float)x / (float)WAVE_PIX_PER_PERIOD;

        /* y(x) = Σ A_k · cos(2π·(k+1)·t_norm + eff_phi_k) */
        float y = 0.0f;
        for (int k = 0; k < THD_MAX_HARMONIC; ++k) {
            float A = r->harmonic[k];
            if (A < 1e-3f) continue;
            float ang = TWO_PI * (float)(k + 1) * t_norm + eff_phi[k];
            y += A * cosf(ang);
        }

        int y_pix = WAVE_CENTER_Y - (int)(y * scale_y);
        /* 硬裁剪，避免画出区域外 */
        if (y_pix < WAVE_Y)             y_pix = WAVE_Y;
        if (y_pix > WAVE_Y + WAVE_H - 1) y_pix = WAVE_Y + WAVE_H - 1;

        /* 相邻两点连线（更像示波器）；第一列没前一点，只画点 */
        if (prev_y < 0) {
            ILI9341_SetPointPixel(x, y_pix);
        } else {
            ILI9341_DrawLine(x - 1, prev_y, x, y_pix);
        }
        prev_y = y_pix;
    }
}

static void thd_enter(void)
{
    LCD_Clear(BLACK);
    LCD_DrawTextC(   4,   0, YELLOW, BLACK, "THD Meter (Goertzel + Reconstruction)");

    /* 波形区域先画一条中线占位 */
    LCD_FillRect(WAVE_X, WAVE_CENTER_Y, WAVE_W, 1, BLUE);

    /* 谐波标签 H1..H5 */
    for (int k = 0; k < THD_MAX_HARMONIC; ++k) {
        LCD_DrawTextf(4, THD_H1_Y + k * THD_ROW_DY, WHITE, BLACK,
                      "H%d", k + 1);
    }

    for (int k = 0; k < THD_MAX_HARMONIC; ++k) s_last_bar_w[k] = 0;
    s_last_wave_frame = 0xFFFFFFFFU;    /* 强制首帧重画波形 */
}

static void thd_tick(void)
{
    const thd_result_t *r = thd_get_result();

    /* --- 波形：只在有新帧时重画 --- */
    if (r->frame_id != s_last_wave_frame) {
        s_last_wave_frame = r->frame_id;
        wave_redraw(r);
    }

    /* --- 第二行：f0 / THD --- */
    LCD_DrawTextf(  4,  80, GREEN, BLACK, "f0:%6.1f Hz  ", (double)r->f0_hz);
    LCD_DrawTextf(170,  80, r->thd_percent > 5.0f ? RED : GREEN, BLACK,
                   "THD:%5.2f %%  ", (double)r->thd_percent);

    /* --- 谐波条 + 数值 --- */
    float h1 = r->harmonic[0];
    if (h1 < 1e-3f) h1 = 1.0f;

    for (int k = 0; k < THD_MAX_HARMONIC; ++k) {
        float ratio = r->harmonic[k] / h1;
        if (ratio > 1.0f) ratio = 1.0f;
        int   w   = (int)(ratio * THD_BAR_MAX_W);
        int   y   = THD_H1_Y + k * THD_ROW_DY;

        /* 差量重画：只擦掉/补上变化部分，减少闪烁 */
        if (w < s_last_bar_w[k]) {
            LCD_FillRect(THD_BAR_X + w, y,
                         s_last_bar_w[k] - w, THD_BAR_H, BLACK);
        } else if (w > s_last_bar_w[k]) {
            LCD_FillRect(THD_BAR_X + s_last_bar_w[k], y,
                         w - s_last_bar_w[k], THD_BAR_H, GREEN);
        }
        s_last_bar_w[k] = w;

        LCD_DrawTextf(THD_VAL_X, y, WHITE, BLACK, "%7.1f", (double)r->harmonic[k]);
    }

    /* --- 状态栏 --- */
    LCD_DrawTextf(4, 210, WHITE, BLACK,
                  "#%lu  Fs=%lu Hz  %s   ",
                  (unsigned long)r->frame_id,
                  (unsigned long)r->fs_hz,
                  thd_is_running() ? "RUN " : "STOP");
}

static const ui_page_t s_thd_page = {
    .name  = "thd",
    .enter = thd_enter,
    .tick  = thd_tick,
};

/* ============================================================
 *  Page registration & switching
 * ============================================================ */

void ui_register_page(const ui_page_t *p)
{
    if (!p || s_page_count >= UI_MAX_PAGES) return;
    s_pages[s_page_count++] = p;
}

bool ui_switch_to(const char *name)
{
    for (uint8_t i = 0; i < s_page_count; ++i) {
        if (strcmp(s_pages[i]->name, name) == 0) {
            s_current = s_pages[i];
            s_dirty   = true;
            return true;
        }
    }
    return false;
}

/* ============================================================
 *  Terminal 命令：ui.page <name>
 * ============================================================ */

static int cmd_ui_page(int argc, char **argv)
{
    if (argc < 2) {
        term_printf("usage: ui.page <name>\r\n");
        term_printf("pages:");
        for (uint8_t i = 0; i < s_page_count; ++i)
            term_printf(" %s", s_pages[i]->name);
        term_printf("\r\n");
        return -1;
    }
    if (!ui_switch_to(argv[1])) {
        term_printf("unknown page: %s\r\n", argv[1]);
        return -1;
    }
    return 0;
}
static term_cmd_t s_c_ui_page = { "ui.page", cmd_ui_page, "<name> switch page", 0 };

/* ============================================================
 *  Init / Task
 * ============================================================ */

void ui_init(void)
{
    LCD_Init();
    ui_register_page(&s_welcome);
    ui_register_page(&s_thd_page);
    ui_switch_to("welcome");
    term_register(&s_c_ui_page);
}

void ui_task(void)
{
    if (!s_current) return;
    if (s_dirty) {
        if (s_current->enter) s_current->enter();
        s_dirty = false;
    }
    if (s_current->tick) s_current->tick();
}
