#include "ttask.h"

#include "ttask_core.h"
#include "idt.h"
#include "tss.h"
#include "tstring.h"
#include "tmalloc.h"
#include "tpmm.h"
#include "tvmm.h"

/* Fixed task table: slot 0 is always the idle task (the boot context). */
static ttask_t tasks[TTASK_MAX_TASKS];
static int scheduler_ready = 0;

ttask_t *current_task = 0;
ttask_t *next_task = 0;

extern void ttask_switch_ctx(void);
extern void ttask_entry_iret(void);
extern char stack_top;          /* boot stack top (boot/boot.asm) */

static void ttask_init_stack(ttask_t *t);
static void ttask_entry(void);
static void ttask_free_resources(ttask_t *t);

/* Select the next task: round-robin scan starting after the current task,
 * preferring higher priority. When the current task's timeslice has run out
 * it is treated as not runnable, so the CPU moves on instead of re-picking
 * it; the scan falls back to it only if nothing else is ready.
 * M7: idle boost — when the idle task is the only READY task, give it a
 * long slice (TTASK_SLICE_IDLE) so we don't waste cycles preempting it. */
static ttask_t *ttask_pick_next(void)
{
    uint8_t state[TTASK_MAX_TASKS], prio[TTASK_MAX_TASKS];
    for (int i = 0; i < TTASK_MAX_TASKS; i++) {
        state[i] = tasks[i].state;
        prio[i] = tasks[i].priority;
    }
    int cur = (int)(current_task - tasks);
    int idx = ttask_pick_index_ex(cur, state, prio, TTASK_MAX_TASKS,
                                  current_task->slice == 0 ? cur : -1);
    return idx >= 0 ? &tasks[idx] : current_task;
}

static void ttask_pick_switch(void)
{
    ttask_t *next = ttask_pick_next();
    if (next && next != current_task) {
        next->slice = ttask_slice_ticks(next->priority);
        /* ring-3 transitions land on this task's kernel stack (TSS rsp0) */
        tss_set_rsp0(next->stack ? (uint64_t)next->stack + next->stack_size
                                 : (uintptr_t)&stack_top);
        next_task = next;
        ttask_switch_ctx();       /* resumes in next_task; returns later */

        /* Back on the resumed task. Move CR3 to the address space it runs
         * in; kernel tasks share the kernel space, user tasks own one. */
        vmm_aspace_t *want = next->user_mode
                                 ? (vmm_aspace_t *)next->aspace
                                 : vmm_aspace_kernel();
        if (want && vmm_aspace_current() != want)
            vmm_aspace_switch(want);
    }
}

void ttask_init(void)
{
    ttask_t *idle = &tasks[0];
    tmemset(idle, 0, sizeof *idle);
    tstrncpy(idle->name, "idle", sizeof idle->name - 1);
    idle->state = TTASK_READY;
    idle->priority = 0;
    idle->slice = 0;              /* never preempted away on its own */
    idle->context = 0;            /* captured on the first switch away */
    current_task = idle;
    scheduler_ready = 1;
}

/* Build a fresh task's initial stack. Kernel tasks get the standard iretq
 * frame into ttask_entry; user tasks get a ring-3 frame that iretq's
 * straight into the program at its user_rsp with user segments loaded. */
static void ttask_init_stack(ttask_t *t)
{
    uintptr_t top = ((uintptr_t)t->stack + t->stack_size) & ~(uintptr_t)15;
    uint64_t *sp = (uint64_t *)top;

    if (t->user_mode) {
        *--sp = 0x23;                        /* ss  (user data, RPL 3) */
        *--sp = (uint64_t)(t->user_rsp);     /* ring-3 stack pointer */
        *--sp = 0x202;                       /* rflags: IF set */
        *--sp = 0x1B;                        /* cs  (user code, RPL 3) */
        *--sp = (uint64_t)t->fn;             /* rip = user entry point */
    } else {
        *--sp = 0x10;                        /* ss  (kernel data) */
        *--sp = (uint64_t)(top - 8);         /* rsp after iretq (ABI entry) */
        *--sp = 0x202;                       /* rflags: IF set */
        *--sp = 0x08;                        /* cs  (kernel code) */
        *--sp = (uint64_t)&ttask_entry;      /* rip */
    }
    *--sp = (uint64_t)&ttask_entry_iret; /* return address for `ret` */
    *--sp = 0;  *--sp = 0;               /* rbp, rbx */
    *--sp = 0;  *--sp = 0;               /* r12, r13 */
    *--sp = 0;  *--sp = 0;               /* r14, r15 */
    t->context = (uint64_t)sp;
}

/* Release everything a finished task held, so its slot is cleanly reusable:
 * the kernel stack, and (for user tasks) the address space it owned. */
static void ttask_free_resources(ttask_t *t)
{
    if (t->stack) {
        tfree(t->stack);
        t->stack = 0;
    }
    if (t->user_mode && t->aspace) {
        vmm_aspace_destroy((vmm_aspace_t *)t->aspace);
        t->aspace = 0;
    }
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

    if (t->state == TTASK_EXITED)
        ttask_free_resources(t);

    tmemset(t, 0, sizeof *t);
    tstrncpy(t->name, name, sizeof t->name - 1);
    t->state = TTASK_READY;
    t->priority = 1;
    t->slice = ttask_slice_ticks(t->priority);
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

ttask_t *ttask_create_user(const char *name, uintptr_t entry,
                           struct vmm_aspace *aspace, uintptr_t user_rsp)
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

    if (t->state == TTASK_EXITED)
        ttask_free_resources(t);

    tmemset(t, 0, sizeof *t);
    tstrncpy(t->name, name, sizeof t->name - 1);
    t->state = TTASK_READY;
    t->priority = 2;             /* interactive: user programs get a longer slice */
    t->slice = ttask_slice_ticks(t->priority);
    t->fn = (ttask_fn_t)entry;       /* stored, never called as a C fn */
    t->stack = tmalloc(TTASK_STACK_SIZE);
    if (!t->stack) {
        t->state = TTASK_FREE;
        return 0;
    }
    t->stack_size = TTASK_STACK_SIZE;
    t->user_mode = 1;
    t->aspace = aspace;
    t->user_rsp = user_rsp;
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
    unsigned long flags;
    __asm__ volatile("pushfq\n\tpopq %0" : "=r"(flags));
    __asm__ volatile("cli");
    ttask_pick_switch();
    if (flags & (1ul << 9))
        __asm__ volatile("sti");
}

uint32_t ttask_self(void)
{
    if (!scheduler_ready || !current_task)
        return TTASK_SELF_NONE;
    return (uint32_t)(current_task - tasks);
}

/* Park the calling task until ttask_wake() makes it runnable again. The
 * caller holds a critical section; registration (waiter list) and the state
 * flip happen atomically so no wake can be lost in between. */
void ttask_block(void)
{
    if (!scheduler_ready || !current_task)
        return;
    unsigned long flags;
    __asm__ volatile("pushfq\n\tpopq %0" : "=r"(flags));
    __asm__ volatile("cli");
    current_task->state = TTASK_BLOCKED;
    ttask_t *next = ttask_pick_next();
    if (!next || next == current_task) {
        current_task->state = TTASK_READY;   /* nobody else runnable */
        if (flags & (1ul << 9))
            __asm__ volatile("sti");
        return;
    }
    next_task = next;
    ttask_switch_ctx();                       /* resumes on wake */
    if (flags & (1ul << 9))
        __asm__ volatile("sti");
}

/* Mark a blocked task runnable. Must be called with interrupts disabled
 * (the IPC module wakes from inside its critical section). */
void ttask_wake(uint32_t id)
{
    if (id >= TTASK_MAX_TASKS)
        return;
    if (tasks[id].state == TTASK_BLOCKED)
        tasks[id].state = TTASK_READY;
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
    if (current_task->slice > 0)
        current_task->slice--;
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

void ttask_set_priority(uint32_t id, uint8_t prio)
{
    if (id >= TTASK_MAX_TASKS)
        return;
    ttask_t *t = &tasks[id];
    if (t->state == TTASK_FREE || t->state == TTASK_EXITED)
        return;
    t->priority = prio;
    t->slice = ttask_slice_ticks(prio);
}

uint8_t ttask_get_priority(uint32_t id)
{
    if (id >= TTASK_MAX_TASKS)
        return 0;
    return tasks[id].priority;
}
