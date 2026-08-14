#include "ttask.h"

#include "ttask_core.h"
#include "idt.h"
#include "tstring.h"
#include "tmalloc.h"

/* Fixed task table: slot 0 is always the idle task (the boot context). */
static ttask_t tasks[TTASK_MAX_TASKS];
static int scheduler_ready = 0;

ttask_t *current_task = 0;
ttask_t *next_task = 0;

extern void ttask_switch_ctx(void);
extern void ttask_entry_iret(void);

static void ttask_init_stack(ttask_t *t);
static void ttask_entry(void);

/* Select the next task: round-robin scan starting after the current task,
 * preferring higher priority. Falls back to the current task when nothing
 * else is ready. */
static ttask_t *ttask_pick_next(void)
{
    uint8_t state[TTASK_MAX_TASKS], prio[TTASK_MAX_TASKS];
    for (int i = 0; i < TTASK_MAX_TASKS; i++) {
        state[i] = tasks[i].state;
        prio[i] = tasks[i].priority;
    }
    int idx = ttask_pick_index((int)(current_task - tasks), state, prio,
                               TTASK_MAX_TASKS);
    return idx >= 0 ? &tasks[idx] : current_task;
}

static void ttask_pick_switch(void)
{
    ttask_t *next = ttask_pick_next();
    if (next && next != current_task) {
        next_task = next;
        ttask_switch_ctx();       /* resumes in next_task; returns later */
    }
}

void ttask_init(void)
{
    ttask_t *idle = &tasks[0];
    tmemset(idle, 0, sizeof *idle);
    tstrncpy(idle->name, "idle", sizeof idle->name - 1);
    idle->state = TTASK_READY;
    idle->priority = 0;
    idle->context = 0;            /* captured on the first switch away */
    current_task = idle;
    scheduler_ready = 1;
}

/* Build a fresh task's initial stack:
 *   [callee-saved block][ret: ttask_entry_iret][iretq frame: rip=ttask_entry]
 * so the first switch into the task pops the block, `ret`s to the iretq
 * trampoline, and starts running ttask_entry on its own stack. */
static void ttask_init_stack(ttask_t *t)
{
    uintptr_t top = ((uintptr_t)t->stack + t->stack_size) & ~(uintptr_t)15;
    uint64_t *sp = (uint64_t *)top;

    *--sp = 0x10;                        /* ss  (kernel data) */
    *--sp = (uint64_t)(top - 8);         /* rsp after iretq (ABI entry) */
    *--sp = 0x202;                       /* rflags: IF set */
    *--sp = 0x08;                        /* cs  (kernel code) */
    *--sp = (uint64_t)&ttask_entry;      /* rip */
    *--sp = (uint64_t)&ttask_entry_iret; /* return address for `ret` */
    *--sp = 0;  *--sp = 0;               /* rbp, rbx */
    *--sp = 0;  *--sp = 0;               /* r12, r13 */
    *--sp = 0;  *--sp = 0;               /* r14, r15 */
    t->context = (uint64_t)sp;
}

ttask_t *ttask_create(const char *name, ttask_fn_t fn, void *arg)
{
    if (!scheduler_ready)
        return 0;

    ttask_t *t = 0;
    for (int i = 0; i < TTASK_MAX_TASKS; i++) {
        if (tasks[i].state == TTASK_FREE || tasks[i].state == TTASK_EXITED) {
            t = &tasks[i];
            break;
        }
    }
    if (!t)
        return 0;

    if (t->state == TTASK_EXITED && t->stack)
        tfree(t->stack);

    tmemset(t, 0, sizeof *t);
    tstrncpy(t->name, name, sizeof t->name - 1);
    t->state = TTASK_READY;
    t->priority = 1;
    t->fn = fn;
    t->arg = arg;
    t->stack = tmalloc(TTASK_STACK_SIZE);
    if (!t->stack) {
        t->state = TTASK_FREE;
        return 0;
    }
    t->stack_size = TTASK_STACK_SIZE;
    ttask_init_stack(t);
    return t;
}

/* Every task starts here (via iretq): run fn(arg), then exit. */
static void ttask_entry(void)
{
    if (current_task->fn)
        current_task->fn(current_task->arg);
    ttask_exit();
}

void ttask_yield(void)
{
    if (!scheduler_ready)
        return;
    __asm__ volatile("cli");
    ttask_pick_switch();
    __asm__ volatile("sti");
}

void ttask_exit(void) __attribute__((noreturn));
void ttask_exit(void)
{
    __asm__ volatile("cli");
    current_task->state = TTASK_EXITED;
    ttask_pick_switch();
    __asm__ volatile("hlt");          /* unreachable safety net */
    for (;;) { }
}

void ttask_tick(void)
{
    if (!scheduler_ready)
        return;
    current_task->ticks++;
    ttask_pick_switch();
}

int ttask_ready(void)
{
    return scheduler_ready;
}

int ttask_count(void)
{
    int n = 0;
    for (int i = 0; i < TTASK_MAX_TASKS; i++)
        if (tasks[i].state != TTASK_FREE)
            n++;
    return n;
}

ttask_t *ttask_at(int i)
{
    return (i >= 0 && i < TTASK_MAX_TASKS) ? &tasks[i] : 0;
}
