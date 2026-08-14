#ifndef TTASK_H
#define TTASK_H

#include <stdint.h>
#include <stddef.h>

#define TTASK_MAX_TASKS   32
#define TTASK_NAME_LEN    16
#define TTASK_STACK_SIZE  (8 * 1024)

typedef void (*ttask_fn_t)(void *arg);

/* Scheduler states. TTASK_FREE slots are reusable; TTASK_BLOCKED tasks are
 * parked on an IPC primitive until ttask_wake() makes them runnable again. */
enum {
    TTASK_FREE = 0,
    TTASK_READY,
    TTASK_RUNNING,
    TTASK_EXITED,
    TTASK_BLOCKED,
};

/* A schedulable kernel task. `context` is the saved stack pointer that
 * boot/task_switch.asm reads/writes; it MUST stay the first member. */
typedef struct ttask {
    uint64_t   context;
    char       name[TTASK_NAME_LEN];
    uint8_t    state;
    uint8_t    priority;
    uint32_t   ticks;        /* CPU time consumed (PIT ticks) */
    ttask_fn_t fn;           /* entry function (runs once) */
    void      *arg;
    void      *stack;        /* tmalloc'd stack; freed when the slot is reused */
    size_t     stack_size;
} ttask_t;

/* Boot the scheduler: the calling context becomes the idle task. */
void ttask_init(void);

/* Create a task that runs fn(arg); 0 when the table is full. */
ttask_t *ttask_create(const char *name, ttask_fn_t fn, void *arg);

void ttask_yield(void);
void ttask_exit(void) __attribute__((noreturn));

/* Blocking support (M6b IPC): ttask_block() parks the calling task until
 * ttask_wake() marks it runnable again. ttask_wake() must be called with
 * interrupts disabled (the IPC module holds a critical section). */
#define TTASK_SELF_NONE 0xFFFFFFFFu
uint32_t ttask_self(void);
void ttask_block(void);
void ttask_wake(uint32_t id);

/* Called from the PIT handler; preempts the current task. */
void ttask_tick(void);

int ttask_ready(void);
int ttask_count(void);
ttask_t *ttask_at(int i);

/* Scheduler internals shared with boot/task_switch.asm. */
extern ttask_t *current_task;
extern ttask_t *next_task;
void ttask_switch_ctx(void);
void ttask_entry_iret(void);

#endif
