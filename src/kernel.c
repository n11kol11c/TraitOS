#include <stdarg.h>
#include <stdint.h>

#include "vga.h"
#include "serial.h"
#include "keyboard.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "kmalloc.h"

/* Kernel console printf (KumOS-style, VGA only; use klog() for serial). */
static void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    while (*fmt) {
        char c = *fmt++;
        if (c != '%') {
            vga_putchar(c);
            continue;
        }
        if (*fmt == '%') {
            fmt++;
            vga_putchar('%');
        } else if (*fmt == 's') {
            fmt++;
            const char *s = va_arg(ap, const char *);
            vga_puts(s ? s : "(null)");
        } else if (*fmt == 'd') {
            fmt++;
            int v = va_arg(ap, int);
            if (v < 0) {
                vga_putchar('-');
                vga_put_dec((uint32_t)(0 - (uint32_t)v));
            } else {
                vga_put_dec((uint32_t)v);
            }
        } else if (*fmt == 'u') {
            fmt++;
            vga_put_dec(va_arg(ap, uint32_t));
        } else if (*fmt == 'x') {
            fmt++;
            vga_put_hex(va_arg(ap, uint32_t));
        } else if (*fmt == 'p') {
            fmt++;
            vga_puts("0x");
            vga_put_hex((uint32_t)(uintptr_t)va_arg(ap, void *));
        } else if (*fmt == 'c') {
            fmt++;
            vga_putchar((char)va_arg(ap, int));
        } else {
            vga_putchar('%');
        }
    }
    va_end(ap);
}

void kernel_main(void)
{
    char line[128];
    int line_len = 0;
    uint32_t last_sec = 0;

    line[0] = '\0';

    vga_init();
    serial_init(COM1);
    gdt_init();
    idt_init();
    timer_init(100);
    keyboard_init();

    klog("TraitOS v0.1.0 booted on x86_64\n");
    klog("interrupts: IDT + PIC, PIT @100Hz, PS/2 keyboard\n");

    kprintf("===============================================\n");
    kprintf(" TraitOS v0.1.0 - RAM-resident, amnesic OS\n");
    kprintf("===============================================\n\n");

    kprintf(" arch      : x86_64\n");
    kprintf(" boot      : GRUB2 / Multiboot2 (BIOS + UEFI)\n");
    kprintf(" storage   : none - runs entirely from RAM\n\n");

    kprintf(" Type something and press Enter. Uptime logs to serial.\n");
    kprintf("   > ");

    __asm__ volatile("sti");

    for (;;) {
        __asm__ volatile("hlt");

        uint32_t now = timer_ticks();
        if (now / 100 != last_sec) {
            last_sec = now / 100;
            klog("uptime: %us\n", last_sec);
        }

        int c;
        while ((c = keyboard_getchar()) >= 0) {
            if (c == '\n') {
                vga_putchar('\n');
                if (line_len > 0)
                    klog("input: %s\n", line);
                line_len = 0;
                line[0] = '\0';
                vga_puts("   > ");
            } else if (c == '\b') {
                if (line_len > 0) {
                    line_len--;
                    line[line_len] = '\0';
                    vga_puts("\b \b");
                }
            } else if (c >= 32 && line_len < 127) {
                line[line_len++] = (char)c;
                line[line_len] = '\0';
                vga_putchar((char)c);
            }
        }
    }
}
