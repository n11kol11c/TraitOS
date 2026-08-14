#include "tipc.h"

#include "ttask.h"

/* Kernel-facing IPC (M6b): wraps the pure data structures in tipc_core.h
 * with interrupt critical sections and the scheduler's block/wake
 * primitives. Single CPU: cli/sti sections are the lock.
 *
 * Blocking discipline (lost-wakeup safety): a task only ever registers
 * itself in a wait queue while interrupts are disabled, and blocks
 * immediately after (ttask_block() re-enters cli and switches away). The
 * waker pops the queue and calls ttask_wake() while holding the same
 * critical section, so every wake has a waiter to receive it. */

static void lock(void)
{
    __asm__ volatile("cli");
}

static void unlock(void)
{
    __asm__ volatile("sti");
}

int tipc_mailbox_init(tipc_mailbox_t *mb)
{
    if (!mb)
        return -1;
    tipc_core_mb_init(mb);
    return 0;
}

int tipc_send(tipc_mailbox_t *mb, tipc_msg_t *msg)
{
    if (!mb || !msg)
        return -1;
    uint32_t self = ttask_self();
    if (self == TTASK_SELF_NONE)
        return -1;
    msg->sender = self;
    for (;;) {
        lock();
        if (tipc_core_mb_push(mb, msg) == 0) {
            uint32_t w = tipc_core_waitq_pop(&mb->wait_recv);
            if (w != TIPC_NONE)
                ttask_wake(w);
            unlock();
            return 0;
        }
        if (tipc_core_waitq_push(&mb->wait_send, self) != 0) {
            unlock();                    /* waiter list full */
            return -1;
        }
        ttask_block();                   /* registers + parks atomically */
        unlock();
    }
}

int tipc_recv(tipc_mailbox_t *mb, tipc_msg_t *out)
{
    if (!mb || !out)
        return -1;
    uint32_t self = ttask_self();
    if (self == TTASK_SELF_NONE)
        return -1;
    for (;;) {
        lock();
        if (tipc_core_mb_pop(mb, out) == 0) {
            uint32_t w = tipc_core_waitq_pop(&mb->wait_send);
            if (w != TIPC_NONE)
                ttask_wake(w);
            unlock();
            return 0;
        }
        if (tipc_core_waitq_push(&mb->wait_recv, self) != 0) {
            unlock();                    /* waiter list full */
            return -1;
        }
        ttask_block();
        unlock();
    }
}

int tipc_mutex_init(tipc_mutex_t *m)
{
    if (!m)
        return -1;
    tipc_core_mutex_init(m);
    return 0;
}

int tipc_mutex_lock(tipc_mutex_t *m)
{
    if (!m)
        return -1;
    uint32_t self = ttask_self();
    if (self == TTASK_SELF_NONE)
        return -1;
    for (;;) {
        lock();
        if (m->owner == TIPC_NONE) {
            m->owner = self;
            m->depth = 1;
            unlock();
            return 0;
        }
        if (m->owner == self) {
            m->depth++;                  /* recursive lock */
            unlock();
            return 0;
        }
        if (tipc_core_waitq_push(&m->wait, self) != 0) {
            unlock();                    /* waiter list full */
            return -1;
        }
        ttask_block();
        unlock();
    }
}

int tipc_mutex_unlock(tipc_mutex_t *m)
{
    if (!m)
        return -1;
    uint32_t self = ttask_self();
    if (self == TTASK_SELF_NONE)
        return -1;
    lock();
    if (m->owner != self) {
        unlock();                        /* not the holder */
        return -1;
    }
    if (--m->depth == 0) {
        m->owner = TIPC_NONE;
        uint32_t w = tipc_core_waitq_pop(&m->wait);
        if (w != TIPC_NONE)
            ttask_wake(w);
    }
    unlock();
    return 0;
}
