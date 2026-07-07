#include "core/scheduler.h"
#include "stm32f1xx_hal.h"
#include <string.h>

static sched_task_t *g_tasks[SCHED_MAX_TASKS];
static uint8_t       g_count = 0;

/* 用 DWT CYCCNT 做微秒级计时（M3 内核可用）。DWT 由 HAL 在启动时使能。 */
static inline uint32_t dwt_us(void)
{
    /* SystemCoreClock 单位 Hz，CYCCNT 每周期加 1 */
    return DWT->CYCCNT / (SystemCoreClock / 1000000U);
}

static void dwt_enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT      = 0;
    DWT->CTRL       |= DWT_CTRL_CYCCNTENA_Msk;
}

void sched_init(void)
{
    memset(g_tasks, 0, sizeof(g_tasks));
    g_count = 0;
    dwt_enable();
}

bool sched_register(sched_task_t *t)
{
    if (!t || !t->run || g_count >= SCHED_MAX_TASKS) return false;
    t->last_tick = 0;
    t->max_us    = 0;
    t->run_count = 0;
    if (!t->name) t->name = "?";
    t->enabled = true;              /* 注册即启用；调用方可再关闭 */
    g_tasks[g_count++] = t;
    return true;
}

bool sched_set_enabled(const char *name, bool en)
{
    for (uint8_t i = 0; i < g_count; ++i) {
        if (strcmp(g_tasks[i]->name, name) == 0) {
            g_tasks[i]->enabled = en;
            return true;
        }
    }
    return false;
}

void sched_reset_stats(void)
{
    for (uint8_t i = 0; i < g_count; ++i) {
        g_tasks[i]->max_us    = 0;
        g_tasks[i]->run_count = 0;
    }
}

const sched_task_t *sched_iter(uint8_t idx)
{
    return (idx < g_count) ? g_tasks[idx] : (const sched_task_t *)0;
}

uint8_t sched_count(void) { return g_count; }

void sched_run_forever(void)
{
    for (;;) {
        uint32_t now = HAL_GetTick();
        for (uint8_t i = 0; i < g_count; ++i) {
            sched_task_t *t = g_tasks[i];
            if (!t->enabled) continue;

            if (t->period_ms == 0 ||
                (uint32_t)(now - t->last_tick) >= t->period_ms) {

                uint32_t t0 = dwt_us();
                t->run();
                uint32_t dt = dwt_us() - t0;
                if (dt > t->max_us) t->max_us = dt;
                t->run_count++;
                t->last_tick = now;
            }
        }
    }
}
