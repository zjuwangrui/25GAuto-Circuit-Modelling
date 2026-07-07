#ifndef __CORE_SCHEDULER_H
#define __CORE_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

/*
 * 协作式时间片调度器
 *
 * 约束：
 *   1. 任务函数必须非阻塞。禁止 HAL_Delay / while 等外设完成。
 *   2. 单次执行时间应远小于最短周期。
 *   3. 长耗时操作必须状态机化，分帧完成。
 */

#define SCHED_MAX_TASKS  16

typedef void (*sched_task_fn_t)(void);

typedef struct {
    sched_task_fn_t  run;          /* 任务函数                       */
    uint32_t         period_ms;    /* 0 = 每轮都跑                   */
    uint32_t         last_tick;    /* 内部使用，注册时清零           */
    const char      *name;         /* 用于 terminal 列表 / 统计      */
    bool             enabled;      /* false 可临时禁用               */
    uint32_t         max_us;       /* 统计：单次最大执行时间 (us)    */
    uint32_t         run_count;    /* 统计：累计执行次数             */
} sched_task_t;

void  sched_init(void);
bool  sched_register(sched_task_t *t);      /* t 指针必须为静态存储 */
void  sched_run_forever(void);              /* 不返回                */

bool  sched_set_enabled(const char *name, bool en);
void  sched_reset_stats(void);

/* 供 terminal 遍历用（返回 NULL 结束） */
const sched_task_t *sched_iter(uint8_t idx);
uint8_t             sched_count(void);

#endif /* __CORE_SCHEDULER_H */
