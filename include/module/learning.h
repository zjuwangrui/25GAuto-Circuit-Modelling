#ifndef __MODULE_LEARNING_H
#define __MODULE_LEARNING_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  module/learning —— 未知电路"学习"业务模块 (空壳阶段)
 * ===========================================================================
 *
 *  目标: 触发一次"学习", 让某个后端算法测出待测电路 (滤波器) 的类型和参数,
 *        结果供 panel_ctrl 推到串口屏显示.
 *
 *  当前实现: **只状态机, 不自动 DONE**.
 *      learning_start() → state = RUNNING
 *      learning_task()   → 什么也不做
 *      调用方需要自己后续填算法, 或在别处置 state = DONE + 写 result.
 *
 *  API 供 panel_ctrl:
 *      learning_start()        触发
 *      learning_stop ()        中止, 回 IDLE
 *      learning_get_state()    IDLE/RUNNING/DONE/FAILED
 *      learning_get_result()   拿最新结果结构体 (state=DONE 才有意义)
 * ===========================================================================
 */

typedef enum {
    LEARN_IDLE    = 0,
    LEARN_RUNNING,
    LEARN_DONE,
    LEARN_FAILED,
} learn_state_t;

typedef enum {
    FILTER_UNKNOWN  = 0,
    FILTER_LOWPASS,
    FILTER_HIGHPASS,
    FILTER_BANDPASS,
    FILTER_BANDSTOP,
    FILTER_ALLPASS,
} filter_type_t;

typedef struct {
    filter_type_t type;
    float         params[4];    /* 通用系数, 具体含义随 type: 截止频率/Q/增益/... */
    uint8_t       n_params;     /* params 里前 n_params 个有效 */
} learn_result_t;

/* 生命周期 */
void  learning_init(void);
void  learning_task(void);           /* 注册到调度器 (目前是空转) */

/* 控制 */
bool  learning_start(void);          /* 已在跑返回 false */
void  learning_stop (void);

/* 查询 */
learn_state_t         learning_get_state (void);
const learn_result_t *learning_get_result(void);

/* 供后端算法填结果时用 (当前空壳不用, 留给你以后接入) */
void learning_set_result(const learn_result_t *r);
void learning_set_state (learn_state_t s);

/* 工具: 滤波类型 → 显示字符串 (中文短标签, 送屏用) */
const char *learning_filter_name(filter_type_t t);

#endif /* __MODULE_LEARNING_H */
