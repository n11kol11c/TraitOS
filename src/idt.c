#include "idt.h"

#include "serial.h"
#include "vga.h"

#include <stdint.h>

typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

#define KERNEL_CODE_SELECTOR 0x08
#define IDT_FLAG_GATE64      0x8E /* present, ring 0, 64-bit interrupt gate */

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

#define ISR_DECL(n) extern void isr##n(void);
#define IRQ_DECL(n) extern void irq##n(void);
ISR_DECL(0)  ISR_DECL(1)  ISR_DECL(2)  ISR_DECL(3)
ISR_DECL(4)  ISR_DECL(5)  ISR_DECL(6)  ISR_DECL(7)
ISR_DECL(8)  ISR_DECL(9)  ISR_DECL(10) ISR_DECL(11)
ISR_DECL(12) ISR_DECL(13) ISR_DECL(14) ISR_DECL(15)
ISR_DECL(16) ISR_DECL(17) ISR_DECL(18) ISR_DECL(19)
ISR_DECL(20) ISR_DECL(21) ISR_DECL(22) ISR_DECL(23)
ISR_DECL(24) ISR_DECL(25) ISR_DECL(26) ISR_DECL(27)
ISR_DECL(28) ISR_DECL(29) ISR_DECL(30) ISR_DECL(31)
IRQ_DECL(0)  IRQ_DECL(1)  IRQ_DECL(2)  IRQ_DECL(3)
IRQ_DECL(4)  IRQ_DECL(5)  IRQ_DECL(6)  IRQ_DECL(7)
IRQ_DECL(8)  IRQ_DECL(9)  IRQ_DECL(10) IRQ_DECL(11)
IRQ_DECL(12) IRQ_DECL(13) IRQ_DECL(14) IRQ_DECL(15)

static idt_entry_t idt[256];
static irq_handler_t irq_handlers[16];

static const char *const exception_names[32] = {
    "Divide Error",            "Debug",              "NMI",
    "Breakpoint",              "Overflow",           "BOUND Range Exceeded",
    "Invalid Opcode",          "Device Not Available",
    "Double Fault",            "Coprocessor Segment Overrun",
    "Invalid TSS",             "Segment Not Present",
    "Stack-Segment Fault",     "General Protection", "Page Fault",
    "Reserved",                "x87 FPU Error",     "Alignment Check",
    "Machine Check",           "SIMD FP Exception", "Virtualization Exception",
    "Control Protection",      "Reserved",           "Reserved",
    "Reserved",                "Reserved",           "Reserved",
    "Reserved",                "Reserved",           "Reserved",
    "Reserved",                "Reserved",
};

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void idt_set_gate(int index, uint64_t base, uint16_t selector,
                         uint8_t flags)
{
    idt[index].base_low = (uint16_t)(base & 0xFFFF);
    idt[index].base_mid = (uint16_t)((base >> 16) & 0xFFFF);
    idt[index].base_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[index].selector = selector;
    idt[index].ist = 0;
    idt[index].flags = flags;
    idt[index].zero = 0;
}

/* Remap the 8259A PIC so IRQs 0-15 land on vectors 32-47 (above the
   32 x86 exceptions). */
static void pic_remap(void)
{
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);
    outb(PIC1_DATA, 0x20);   /* master offset = 32 */
    outb(PIC2_DATA, 0x28);   /* slave  offset = 40 */
    outb(PIC1_DATA, 0x04);   /* master cascade line */
    outb(PIC2_DATA, 0x02);   /* slave cascade line */
    outb(PIC1_DATA, 0x01);   /* 8086 mode */
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0x00);   /* unmask all IRQs */
    outb(PIC2_DATA, 0x00);
}

void idt_init(void)
{
    idt_ptr_t ptr;

    for (int i = 0; i < 256; i++)
        idt_set_gate(i, 0, 0, 0);

    idt_set_gate(0, (uint64_t)isr0, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(1, (uint64_t)isr1, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(2, (uint64_t)isr2, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(3, (uint64_t)isr3, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(4, (uint64_t)isr4, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(5, (uint64_t)isr5, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(6, (uint64_t)isr6, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(7, (uint64_t)isr7, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(8, (uint64_t)isr8, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(9, (uint64_t)isr9, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(10, (uint64_t)isr10, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(11, (uint64_t)isr11, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(12, (uint64_t)isr12, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(13, (uint64_t)isr13, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(14, (uint64_t)isr14, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(15, (uint64_t)isr15, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(16, (uint64_t)isr16, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(17, (uint64_t)isr17, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(18, (uint64_t)isr18, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(19, (uint64_t)isr19, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(20, (uint64_t)isr20, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(21, (uint64_t)isr21, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(22, (uint64_t)isr22, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(23, (uint64_t)isr23, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(24, (uint64_t)isr24, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(25, (uint64_t)isr25, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(26, (uint64_t)isr26, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(27, (uint64_t)isr27, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(28, (uint64_t)isr28, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(29, (uint64_t)isr29, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(30, (uint64_t)isr30, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(31, (uint64_t)isr31, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);

    idt_set_gate(32, (uint64_t)irq0, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(33, (uint64_t)irq1, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(34, (uint64_t)irq2, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(35, (uint64_t)irq3, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(36, (uint64_t)irq4, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(37, (uint64_t)irq5, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(38, (uint64_t)irq6, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(39, (uint64_t)irq7, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(40, (uint64_t)irq8, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(41, (uint64_t)irq9, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(42, (uint64_t)irq10, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(43, (uint64_t)irq11, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(44, (uint64_t)irq12, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(45, (uint64_t)irq13, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(46, (uint64_t)irq14, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);
    idt_set_gate(47, (uint64_t)irq15, KERNEL_CODE_SELECTOR, IDT_FLAG_GATE64);

    pic_remap();

    ptr.limit = sizeof(idt) - 1;
    ptr.base = (uint64_t)&idt[0];
    __asm__ volatile("lidt %0" : : "m"(ptr));
}

void irq_register(int irq, irq_handler_t handler)
{
    if (irq >= 0 && irq < 16)
        irq_handlers[irq] = handler;
}

/* Called from boot/isr_stubs.asm for CPU exceptions (< 32). */
void isr_handler(registers_t *r)
{
    if (r->int_no < 32) {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("\nKERNEL PANIC: ");
        vga_puts(exception_names[r->int_no]);
        vga_puts("\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        klog("PANIC: exception %u (%s) at rip=0x%lx\n",
             (uint32_t)r->int_no, exception_names[r->int_no],
             (unsigned long)r->rip);
        for (;;)
            __asm__ volatile("hlt");
    }
}

/* Called from boot/isr_stubs.asm for hardware IRQs (32-47). */
void irq_handler(registers_t *r)
{
    int irq = (int)(r->int_no - 32);
    if (irq >= 0 && irq < 16 && irq_handlers[irq])
        irq_handlers[irq](r);

    /* signal end-of-interrupt to the PIC(s) */
    if (r->int_no >= 40)
        outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}
