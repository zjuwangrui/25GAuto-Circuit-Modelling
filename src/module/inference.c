/*
 * inference.c —— 推理输出业务模块 (空壳)
 *
 * 只维护状态机. 具体推理动作 (信号发生 + 结果处理) 等后端到位再填.
 */

#include "module/inference.h"

static inference_state_t s_state = INFER_IDLE;

void inference_init(void)
{
    s_state = INFER_IDLE;
}

void inference_task(void)
{
    /* 空转 */
}

bool inference_start(void)
{
    if (s_state == INFER_RUNNING) return false;
    s_state = INFER_RUNNING;
    /* TODO: kick off 推理动作 */
    return true;
}

void inference_stop(void)
{
    s_state = INFER_IDLE;
    /* TODO: 停止/清理动作 */
}

inference_state_t inference_get_state(void) { return s_state; }
void              inference_set_state(inference_state_t s) { s_state = s; }
