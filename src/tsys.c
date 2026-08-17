#include "tsys.h"

#include "idt.h"
#include "keyboard.h"
#include "serial.h"
#include "timer.h"
#include "ttask.h"
#include "tipc.h"
#include "tvmm.h"
#include "vga.h"

/* M8 C1: IPC table — maps integer IDs to kernel mailboxes/mutexes.
 * User programs receive IDs (0..TIPC_MAX_OBJ-1) and pass them back
 * through syscalls. The kernel owns the actual objects. */
#define TIPC_MAX_OBJ 32
enum tipc_obj_type { TIPC_OBJ_FREE = 0, TIPC_OBJ_MB, TIPC_OBJ_MUTEX };

static struct {
    enum tipc_obj_type type;
    union {
        tipc_mailbox_t mb;
        tipc_mutex_t   mtx;
    } u;
} tipc_table[TIPC_MAX_OBJ];

/* Dispatch entry for the `int 0x80` gate (vector 128, DPL 3). Called from
 * boot/isr_stubs.asm with the full saved context; the return value goes
 * back through r->rax, which the stub pops into rax before iretq. */
void syscall_handler(registers_t *r)
{
    if ((r->cs & 3) == 0) {
        r->rax = -1;          /* ring-0 `int 0x80` is not a service call */
        return;
    }

    long nr = (long)r->rax;
    long a1 = (long)r->rdi;
    long a2 = (long)r->rsi;
    long a3 = (long)r->rdx;

    switch (nr) {
    case TSYS_WRITE: {
        if (a1 != 1 || a3 <= 0 ||
            !vmm_range_user((uintptr_t)a2, (size_t)a3, 0)) {
            r->rax = -1;
            break;
        }
        const char *s = (const char *)a2;
        size_t n = (size_t)a3;
        __asm__ volatile("stac" ::: "cc");   /* allow ring-3 buffer reads */
        for (size_t i = 0; i < n; i++) {
            vga_putchar(s[i]);
            serial_putchar(COM1, s[i]);
        }
        __asm__ volatile("clac" ::: "cc");
        r->rax = (long)n;
        break;
    }
    case TSYS_EXIT:
        ttask_exit();
        break;                  /* unreachable: ttask_exit never returns */
    case TSYS_GETPID:
        r->rax = (long)ttask_self();
        break;
    case TSYS_YIELD:
        ttask_yield();
        r->rax = 0;
        break;
    case TSYS_SLEEP: {
        if (a1 < 0) {
            r->rax = -1;
            break;
        }
        uint64_t until = (uint64_t)timer_ticks() + (uint64_t)a1;
        while ((int64_t)((uint64_t)timer_ticks() - until) < 0)
            ttask_yield();
        r->rax = 0;
        break;
    }
    case TSYS_GETTICKS:
        r->rax = (long)timer_ticks();
        break;
    /* --- M8 C1: IPC syscalls --- */
    case TSYS_MB_INIT: {
        /* Find a free slot and init a mailbox there. */
        int id = -1;
        for (int i = 0; i < TIPC_MAX_OBJ; i++) {
            if (tipc_table[i].type == TIPC_OBJ_FREE) {
                tipc_table[i].type = TIPC_OBJ_MB;
                if (tipc_mailbox_init(&tipc_table[i].u.mb) != 0) {
                    tipc_table[i].type = TIPC_OBJ_FREE;
                    break;
                }
                id = i;
                break;
            }
        }
        r->rax = (long)id;
        break;
    }
    case TSYS_MB_SEND: {
        /* a1 = mailbox id, a2 = user pointer to tipc_msg_t (64 bytes) */
        long id = a1;
        if (id < 0 || id >= TIPC_MAX_OBJ ||
            tipc_table[id].type != TIPC_OBJ_MB ||
            !vmm_range_user((uintptr_t)a2, sizeof(tipc_msg_t), 0)) {
            r->rax = -1;
            break;
        }
        tipc_msg_t msg;
        __asm__ volatile("stac" ::: "cc");
        __builtin_memcpy(&msg, (void *)a2, sizeof(tipc_msg_t));
        __asm__ volatile("clac" ::: "cc");
        msg.sender = ttask_self();       /* stamp sender, ignore user value */
        r->rax = (long)tipc_send(&tipc_table[id].u.mb, &msg);
        break;
    }
    case TSYS_MB_RECV: {
        /* a1 = mailbox id, a2 = user pointer to tipc_msg_t (output) */
        long id = a1;
        if (id < 0 || id >= TIPC_MAX_OBJ ||
            tipc_table[id].type != TIPC_OBJ_MB ||
            !vmm_range_user((uintptr_t)a2, sizeof(tipc_msg_t), 0)) {
            r->rax = -1;
            break;
        }
        tipc_msg_t out;
        r->rax = (long)tipc_recv(&tipc_table[id].u.mb, &out);
        if (r->rax == 0) {
            __asm__ volatile("stac" ::: "cc");
            __builtin_memcpy((void *)a2, &out, sizeof(tipc_msg_t));
            __asm__ volatile("clac" ::: "cc");
        }
        break;
    }
    case TSYS_MUTEX_INIT: {
        int id = -1;
        for (int i = 0; i < TIPC_MAX_OBJ; i++) {
            if (tipc_table[i].type == TIPC_OBJ_FREE) {
                tipc_table[i].type = TIPC_OBJ_MUTEX;
                if (tipc_mutex_init(&tipc_table[i].u.mtx) != 0) {
                    tipc_table[i].type = TIPC_OBJ_FREE;
                    break;
                }
                id = i;
                break;
            }
        }
        r->rax = (long)id;
        break;
    }
    case TSYS_MUTEX_LOCK: {
        long id = a1;
        if (id < 0 || id >= TIPC_MAX_OBJ ||
            tipc_table[id].type != TIPC_OBJ_MUTEX) {
            r->rax = -1;
            break;
        }
        r->rax = (long)tipc_mutex_lock(&tipc_table[id].u.mtx);
        break;
    }
    case TSYS_MUTEX_UNLOCK: {
        long id = a1;
        if (id < 0 || id >= TIPC_MAX_OBJ ||
            tipc_table[id].type != TIPC_OBJ_MUTEX) {
            r->rax = -1;
            break;
        }
        r->rax = (long)tipc_mutex_unlock(&tipc_table[id].u.mtx);
        break;
    }
    case TSYS_READ: {
        /* a1 = fd (must be 0=stdin), a2 = user buffer, a3 = max length */
        if (a1 != 0 || a3 <= 0 ||
            !vmm_range_user((uintptr_t)a2, (size_t)a3, 1)) {
            r->rax = -1;
            break;
        }
        /* Blocking read: yield until a key is available. */
        int ch;
        while ((ch = keyboard_getchar()) == KEY_NONE)
            ttask_yield();
        if (ch < 0) {
            /* Special key — ignore for now, return 0 bytes. */
            r->rax = 0;
            break;
        }
        __asm__ volatile("stac" ::: "cc");
        *(char *)a2 = (char)ch;
        __asm__ volatile("clac" ::: "cc");
        r->rax = 1;
        break;
    }
    default:
        r->rax = -1;
        break;
    }
}
