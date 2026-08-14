#ifndef TTASK_CORE_H
#define TTASK_CORE_H

#include <stdint.h>

#include "ttask.h"

/* Round-robin pick over a fixed task array: scan starting just after
 * `current`, preferring higher priority, and return the index of the best
 * READY task. Returns -1 when nothing is ready. Pure (no kernel state) so
 * `make smoke` verifies the exact scheduling math the kernel uses. */
static inline int ttask_pick_index(int current, const uint8_t *state,
                                   const uint8_t *prio, int count)
{
    int best = -1;
    int best_prio = -1;
    for (int i = 0; i < count; i++) {
        int idx = (current + 1 + i) % count;
        if (state[idx] != TTASK_READY)
            continue;
        if ((int)prio[idx] > best_prio) {
            best = idx;
            best_prio = (int)prio[idx];
        }
    }
    return best;
}

#endif
