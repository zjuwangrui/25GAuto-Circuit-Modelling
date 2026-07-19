/*
 * learning.c —— 学习业务模块 (空壳)
 *
 * 现只维护状态机 (IDLE/RUNNING/DONE/FAILED), 不自动跳到 DONE.
 * 真实的电路学习算法等后端就绪后填入 learning_task 或异步接入,
 * 完成时调 learning_set_state(LEARN_DONE) + learning_set_result(...) 即可.
 */

#include "module/learning.h"

static learn_state_t   s_state  = LEARN_IDLE;
static learn_result_t  s_result = { FILTER_UNKNOWN, {0}, 0 };

void learning_init(void)
{
    s_state = LEARN_IDLE;
    s_result.type      = FILTER_UNKNOWN;
    s_result.n_params  = 0;
}

void learning_task(void)
{
    /* 空转. 后端算法起来后, 在这里推进采样/拟合状态机. */
}

bool learning_start(void)
{
    if (s_state == LEARN_RUNNING) return false;
    s_state = LEARN_RUNNING;
    /* TODO: 在这里 kick off 学习动作 (启动 ADC 扫频 / 打信号 / 记录响应 ...) */
    return true;
}

void learning_stop(void)
{
    /* 无论是否在跑, 强制回 IDLE */
    s_state = LEARN_IDLE;
}

learn_state_t         learning_get_state (void) { return s_state; }
const learn_result_t *learning_get_result(void) { return &s_result; }

void learning_set_result(const learn_result_t *r)
{
    if (r) s_result = *r;
}

void learning_set_state(learn_state_t s)
{
    s_state = s;
}

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
