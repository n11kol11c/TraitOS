/* Host-side verification of the M6a scheduler pick (round-robin + priority)
 * in src/ttask_core.h. The kernel calls the exact same pure helper when it
 * selects the next task to run. */
#include <stdio.h>
#include <stdint.h>

#include "ttask_core.h"

static int checks = 0;
static int failures = 0;

static void check(int cond, const char *what)
{
    checks++;
    if (!cond) {
        failures++;
        printf("  FAIL %s\n", what);
    }
}

int main(void)
{
    enum { N = 8 };
    uint8_t st[N], pr[N];

    printf("ttask scheduler smoke\n");

    for (int i = 0; i < N; i++) {
        st[i] = TTASK_FREE;
        pr[i] = 0;
    }

    printf("  -- empty table --\n");
    check(ttask_pick_index(0, st, pr, N) == -1, "no ready tasks -> -1");

    printf("  -- single ready task --\n");
    st[3] = TTASK_READY;
    check(ttask_pick_index(0, st, pr, N) == 3, "single ready picked");
    check(ttask_pick_index(7, st, pr, N) == 3, "wraps past table end");

    printf("  -- current is the only ready task --\n");
    check(ttask_pick_index(3, st, pr, N) == 3, "round-trip back to current");

    printf("  -- round robin picks first after current --\n");
    st[3] = TTASK_FREE;
    st[1] = TTASK_READY;
    st[4] = TTASK_READY;
    check(ttask_pick_index(0, st, pr, N) == 1, "first ready after current");
    check(ttask_pick_index(4, st, pr, N) == 1, "wraps to the front");
    check(ttask_pick_index(1, st, pr, N) == 4, "round-robin advances");

    printf("  -- priority beats position --\n");
    pr[1] = 1;
    pr[4] = 5;
    check(ttask_pick_index(0, st, pr, N) == 4, "high priority wins");
    st[6] = TTASK_READY;
    pr[6] = 5;
    check(ttask_pick_index(0, st, pr, N) == 4, "earlier high priority wins");

    printf("  -- exited tasks are skipped --\n");
    st[4] = TTASK_EXITED;
    check(ttask_pick_index(0, st, pr, N) == 6, "exited skipped");

    printf("  -- current is the only ready task --\n");
    for (int i = 0; i < N; i++)
        st[i] = TTASK_FREE;
    st[5] = TTASK_READY;
    check(ttask_pick_index(5, st, pr, N) == 5, "current alone wraps back to itself");
    st[0] = TTASK_READY;
    check(ttask_pick_index(5, st, pr, N) == 0, "wraps to ready at front");

    printf("  -- single-slot table --\n");
    st[0] = TTASK_READY;
    check(ttask_pick_index(0, st, pr, 1) == 0, "count=1 self pick");

    if (failures) {
        printf("TTASK SMOKE TEST FAILED (%d/%d failures)\n", failures, checks);
        return 1;
    }
    printf("TTASK SMOKE TEST PASSED (%d checks)\n", checks);
    return 0;
}
