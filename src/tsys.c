#include "tsys.h"

#include "idt.h"
#include "serial.h"
#include "timer.h"
#include "ttask.h"
#include "tvmm.h"
#include "vga.h"

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
    default:
        r->rax = -1;
        break;
    }
}
