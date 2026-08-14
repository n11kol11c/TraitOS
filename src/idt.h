#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* Full register context passed to interrupt handlers (see boot/isr_stubs.asm
   for the exact stack layout this matches). */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

typedef void (*irq_handler_t)(registers_t *r);

void idt_init(void);
void irq_register(int irq, irq_handler_t handler);

#endif
