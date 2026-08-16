#ifndef TTASK_CORE_H
#define TTASK_CORE_H

#include <stdint.h>

#include "ttask.h"

/* Round-robin pick over a fixed task array: scan starting just after
 * `current`, preferring higher priority, and return the index of the best
 * READY task. Exited and blocked tasks are skipped. Returns -1 when nothing
 * is ready. Pure (no kernel state) so `make smoke` verifies the exact
 * scheduling math the kernel uses. */
static inline int ttask_pick_index_ex(int current, const uint8_t *state,
                                      const uint8_t *prio, int count,
                                      int exclude)
{
    int best = -1;
    int best_prio = -1;
    for (int i = 0; i < count; i++) {
        int idx = (current + 1 + i) % count;
        if (idx == exclude || state[idx] != TTASK_READY)
            continue;
        if ((int)prio[idx] > best_prio) {
            best = idx;
            best_prio = (int)prio[idx];
        }
    }
    return best;
}

static inline int ttask_pick_index(int current, const uint8_t *state,
                                   const uint8_t *prio, int count)
{
    return ttask_pick_index_ex(current, state, prio, count, -1);
}

/* Timeslice length in PIT ticks for a task of a given priority. Higher
 * priority gets a longer slice (fewer preemptions, so interactive and
 * high-priority work keeps the CPU); low-priority work yields often.
 * When a task's slice expires it is pushed to the back of the queue, so
 * every READY task of the same priority still gets its turn.
 *
 * M7: the idle task (prio 0) gets a long slice (32 ticks) when it is the
 * only READY task, avoiding pointless context-switch overhead. Normal
 * priorities keep the exponential mapping: prio N -> 2^N ticks, capped
 * at TTASK_SLICE_MAX. */
#define TTASK_SLICE_MAX  16u
#define TTASK_SLICE_IDLE 32u
static inline uint32_t ttask_slice_ticks(uint8_t prio)
{
    if (prio == 0)
        return TTASK_SLICE_IDLE;
    uint32_t s = 1u << ((unsigned)prio & 7u);
    return s > TTASK_SLICE_MAX ? TTASK_SLICE_MAX : s;
}

/* Determine whether a task is "alone" (no other READY tasks exist) by
 * counting READY slots in the state array. Pure, host-testable. */
static inline int ttask_ready_count_ex(const uint8_t *state, int count)
{
    int n = 0;
    for (int i = 0; i < count; i++)
        if (state[i] == TTASK_READY)
            n++;
    return n;
}

#endif
