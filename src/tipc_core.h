#ifndef TIPC_CORE_H
#define TIPC_CORE_H

/* Pure, scheduler-free IPC data structures (M6b). Every function here is a
 * static inline that only manipulates its struct, so `make smoke` can link
 * them directly into a host test and verify the exact ring/wait-queue math
 * the kernel uses. tipc.c wraps them with interrupt critical sections and
 * the scheduler's block/wake primitives. */

#include <stdint.h>
#include <stddef.h>

#define TIPC_MSG_DATA     56
#define TIPC_QUEUE_SLOTS  8
#define TIPC_WAIT_MAX     16
#define TIPC_NONE         0xFFFFFFFFu

/* One message: sender id (stamped by tipc_send), payload length, payload. */
typedef struct {
    uint32_t sender;
    uint32_t len;
    uint8_t  data[TIPC_MSG_DATA];
} tipc_msg_t;

/* Fixed-size FIFO of blocked task ids. Ring math only; push fails (-1)
 * when full, pop returns TIPC_NONE when empty. */
typedef struct {
    uint32_t id[TIPC_WAIT_MAX];
    uint8_t  head;
    uint8_t  n;
} tipc_waitq_t;

/* Mailbox: bounded message ring plus sender/receiver wait queues. */
typedef struct {
    tipc_msg_t   slot[TIPC_QUEUE_SLOTS];
    uint8_t      head;
    uint8_t      count;
    tipc_waitq_t wait_send;     /* tasks blocked because the ring is full */
    tipc_waitq_t wait_recv;     /* tasks blocked because the ring is empty */
} tipc_mailbox_t;

/* Recursive mutex: owner task id (TIPC_NONE when free), recursion depth,
 * and the FIFO of tasks waiting to acquire it. */
typedef struct {
    uint32_t    owner;
    uint32_t    depth;
    tipc_waitq_t wait;
} tipc_mutex_t;

/* ---- wait queues ------------------------------------------------------ */

static inline void tipc_core_waitq_init(tipc_waitq_t *q)
{
    q->head = 0;
    q->n = 0;
}

static inline int tipc_core_waitq_push(tipc_waitq_t *q, uint32_t id)
{
    if (q->n >= TIPC_WAIT_MAX)
        return -1;
    q->id[(q->head + q->n) % TIPC_WAIT_MAX] = id;
    q->n++;
    return 0;
}

static inline uint32_t tipc_core_waitq_pop(tipc_waitq_t *q)
{
    if (q->n == 0)
        return TIPC_NONE;
    uint32_t id = q->id[q->head];
    q->head = (uint8_t)((q->head + 1) % TIPC_WAIT_MAX);
    q->n--;
    return id;
}

static inline uint8_t tipc_core_waitq_count(const tipc_waitq_t *q)
{
    return q->n;
}

/* ---- mailbox ring ----------------------------------------------------- */

static inline void tipc_core_mb_init(tipc_mailbox_t *mb)
{
    mb->head = 0;
    mb->count = 0;
    tipc_core_waitq_init(&mb->wait_send);
    tipc_core_waitq_init(&mb->wait_recv);
}

static inline int tipc_core_mb_full(const tipc_mailbox_t *mb)
{
    return mb->count >= TIPC_QUEUE_SLOTS;
}

static inline int tipc_core_mb_empty(const tipc_mailbox_t *mb)
{
    return mb->count == 0;
}

static inline int tipc_core_mb_push(tipc_mailbox_t *mb, const tipc_msg_t *m)
{
    if (tipc_core_mb_full(mb))
        return -1;
    mb->slot[(mb->head + mb->count) % TIPC_QUEUE_SLOTS] = *m;
    mb->count++;
    return 0;
}

static inline int tipc_core_mb_pop(tipc_mailbox_t *mb, tipc_msg_t *out)
{
    if (tipc_core_mb_empty(mb))
        return -1;
    *out = mb->slot[mb->head];
    mb->head = (uint8_t)((mb->head + 1) % TIPC_QUEUE_SLOTS);
    mb->count--;
    return 0;
}

/* ---- mutex ------------------------------------------------------------ */

static inline void tipc_core_mutex_init(tipc_mutex_t *m)
{
    m->owner = TIPC_NONE;
    m->depth = 0;
    tipc_core_waitq_init(&m->wait);
}

#endif
