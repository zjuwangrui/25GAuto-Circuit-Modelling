#ifndef __MODULE_INFERENCE_H
#define __MODULE_INFERENCE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  module/inference —— "推理输出" 业务模块 (空壳阶段)
 * ===========================================================================
 *
 *  目标: 基于 learning 学到的电路参数, 让 MCU 做一段特定输出 (比如按学到的
 *        滤波类型反过来送激励信号, 或输出预定序列).
 *
 *  当前实现: **只状态机, 不自动 DONE**. 与 learning 同款设计.
 *
 *  API 供 panel_ctrl:
 *      inference_start()        触发
 *      inference_stop ()        停止, 回 IDLE
 *      inference_get_state()    IDLE / RUNNING / DONE / FAILED
 * ===========================================================================
 */

typedef enum {
    INFER_IDLE    = 0,
    INFER_RUNNING,
    INFER_DONE,
    INFER_FAILED,
} inference_state_t;

void inference_init(void);
void inference_task(void);           /* 注册到调度器 (目前是空转) */

bool inference_start(void);
void inference_stop (void);

inference_state_t inference_get_state(void);

/* 供后端填状态用 */
void inference_set_state(inference_state_t s);

#endif /* __MODULE_INFERENCE_H */
