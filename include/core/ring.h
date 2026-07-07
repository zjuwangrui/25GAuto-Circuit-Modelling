#ifndef __CORE_RING_H
#define __CORE_RING_H

#include <stdint.h>
#include <stdbool.h>

/*
 * 单生产者单消费者字节环形缓冲。容量必须为 2 的幂。
 * 生产端在中断里 push，消费端在任务里 pop —— 无需加锁。
 */

typedef struct {
    uint8_t  *buf;
    uint16_t  mask;       /* size - 1 */
    volatile uint16_t head;  /* 写入位置（生产者） */
    volatile uint16_t tail;  /* 读取位置（消费者） */
} ring_t;

/* size 必须是 2 的幂，例如 64/128/256 */
void     ring_init(ring_t *r, uint8_t *storage, uint16_t size);
bool     ring_push(ring_t *r, uint8_t b);   /* 满了返回 false */
bool     ring_pop (ring_t *r, uint8_t *b);  /* 空了返回 false */
uint16_t ring_available(const ring_t *r);
void     ring_flush(ring_t *r);

#endif /* __CORE_RING_H */
