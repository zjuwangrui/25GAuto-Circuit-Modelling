#include "core/ring.h"

void ring_init(ring_t *r, uint8_t *storage, uint16_t size)
{
    r->buf  = storage;
    r->mask = (uint16_t)(size - 1U);
    r->head = 0;
    r->tail = 0;
}

bool ring_push(ring_t *r, uint8_t b)
{
    uint16_t next = (uint16_t)((r->head + 1U) & r->mask);
    if (next == r->tail) return false;      /* 满 */
    r->buf[r->head] = b;
    r->head = next;
    return true;
}

bool ring_pop(ring_t *r, uint8_t *b)
{
    if (r->head == r->tail) return false;   /* 空 */
    *b = r->buf[r->tail];
    r->tail = (uint16_t)((r->tail + 1U) & r->mask);
    return true;
}

uint16_t ring_available(const ring_t *r)
{
    return (uint16_t)((r->head - r->tail) & r->mask);
}

void ring_flush(ring_t *r)
{
    r->tail = r->head;
}
