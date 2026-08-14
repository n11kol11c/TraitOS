/* Host-side verification of the M6b IPC core in src/tipc_core.h: mailbox
 * ring push/pop/wrap-around, FIFO wait-queue math, and a simulated
 * producer/consumer hand-off. The kernel uses the exact same pure helpers
 * inside interrupt critical sections. */
#include <stdio.h>
#include <stdint.h>

#include "tipc_core.h"

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

static void fill_msg(tipc_msg_t *m, uint32_t seq)
{
    m->sender = 7;
    m->len = 4;
    m->data[0] = (uint8_t)seq;
    m->data[1] = (uint8_t)(seq >> 8);
    m->data[2] = (uint8_t)(seq >> 16);
    m->data[3] = (uint8_t)(seq >> 24);
}

int main(void)
{
    printf("ipc core smoke\n");

    printf("  -- wait queue --\n");
    tipc_waitq_t q;
    tipc_core_waitq_init(&q);
    check(tipc_core_waitq_count(&q) == 0, "empty waitq");
    check(tipc_core_waitq_pop(&q) == TIPC_NONE, "pop on empty -> NONE");
    check(tipc_core_waitq_push(&q, 3) == 0, "push 3");
    check(tipc_core_waitq_push(&q, 8) == 0, "push 8");
    check(tipc_core_waitq_push(&q, 1) == 0, "push 1");
    check(tipc_core_waitq_count(&q) == 3, "count 3");
    check(tipc_core_waitq_pop(&q) == 3, "FIFO pop 3");
    check(tipc_core_waitq_pop(&q) == 8, "FIFO pop 8");
    check(tipc_core_waitq_pop(&q) == 1, "FIFO pop 1");
    check(tipc_core_waitq_pop(&q) == TIPC_NONE, "drained -> NONE");

    printf("  -- wait queue wraps --\n");
    for (uint32_t i = 0; i < TIPC_WAIT_MAX; i++)
        check(tipc_core_waitq_push(&q, i) == 0, "fill waitq");
    check(tipc_core_waitq_push(&q, 99) == -1, "full waitq rejects");
    for (uint32_t i = 0; i < TIPC_WAIT_MAX; i++)
        check(tipc_core_waitq_pop(&q) == i, "wrap keeps FIFO order");
    check(tipc_core_waitq_count(&q) == 0, "waitq drained");
    check(tipc_core_waitq_push(&q, 4) == 0, "reusable after drain");

    printf("  -- mailbox ring --\n");
    tipc_mailbox_t mb;
    tipc_core_mb_init(&mb);
    check(tipc_core_mb_empty(&mb), "init empty");
    check(!tipc_core_mb_full(&mb), "init not full");
    check(tipc_core_mb_pop(&mb, 0) == -1, "pop on empty rejects");

    tipc_msg_t m;
    fill_msg(&m, 0xDEADBEEF);
    check(tipc_core_mb_push(&mb, &m) == 0, "push one");
    check(!tipc_core_mb_empty(&mb), "not empty after push");
    check(tipc_core_mb_pop(&mb, &m) == 0, "pop one");
    check(m.sender == 7 && m.len == 4, "message preserved");
    check(m.data[0] == 0xEF && m.data[3] == 0xDE, "payload preserved");

    printf("  -- mailbox full + wrap --\n");
    for (uint32_t i = 0; i < TIPC_QUEUE_SLOTS; i++) {
        fill_msg(&m, i);
        check(tipc_core_mb_push(&mb, &m) == 0, "fill ring");
    }
    check(tipc_core_mb_full(&mb), "full ring");
    check(tipc_core_mb_push(&mb, &m) == -1, "full ring rejects");
    for (uint32_t i = 0; i < TIPC_QUEUE_SLOTS; i++) {
        check(tipc_core_mb_pop(&mb, &m) == 0, "drain ring");
        check(m.data[0] == (uint8_t)i, "ring keeps order");
    }
    fill_msg(&m, 100);
    check(tipc_core_mb_push(&mb, &m) == 0, "reusable after drain");

    printf("  -- mutex init --\n");
    tipc_mutex_t mu;
    tipc_core_mutex_init(&mu);
    check(mu.owner == TIPC_NONE, "mutex free");
    check(mu.depth == 0, "depth 0");
    check(tipc_core_waitq_count(&mu.wait) == 0, "no waiters");

    printf("  -- producer/consumer hand-off --\n");
    tipc_core_mb_init(&mb);
    tipc_core_waitq_init(&q);
    uint32_t producer = 2;
    tipc_msg_t got;
    int consumed = 0;
    for (uint32_t i = 0; i < TIPC_QUEUE_SLOTS; i++) {
        fill_msg(&m, i);
        check(tipc_core_mb_push(&mb, &m) == 0, "fill ring");
    }
    fill_msg(&m, TIPC_QUEUE_SLOTS);
    check(tipc_core_mb_push(&mb, &m) == -1, "9th push would block producer");
    check(tipc_core_waitq_push(&q, producer) == 0, "producer registered");
    check(tipc_core_mb_pop(&mb, &got) == 0, "consumer drains a slot");
    check(got.data[0] == (uint8_t)consumed, "oldest message first");
    consumed++;
    check(tipc_core_waitq_pop(&q) == producer, "consumer wakes producer");
    check(tipc_core_mb_push(&mb, &m) == 0, "producer resumes after wake");
    while (!tipc_core_mb_empty(&mb)) {
        tipc_core_mb_pop(&mb, &got);
        check(got.data[0] == (uint8_t)consumed, "drain keeps FIFO order");
        consumed++;
    }
    check(consumed == TIPC_QUEUE_SLOTS + 1, "all messages accounted for");
    check(tipc_core_waitq_count(&mb.wait_send) == 0, "no stranded senders");
    check(tipc_core_waitq_count(&mb.wait_recv) == 0, "no stranded receivers");

    if (failures) {
        printf("IPC SMOKE TEST FAILED (%d/%d failures)\n", failures, checks);
        return 1;
    }
    printf("IPC SMOKE TEST PASSED (%d checks)\n", checks);
    return 0;
}
