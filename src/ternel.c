#include <stdarg.h>
#include <stdint.h>

#include "vga.h"
#include "serial.h"
#include "teyboard.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "tmalloc.h"

/* Kernel console printf (KumOS-style, VGA only; use tlog() for serial). */
static void tprintf(const char *fmt, ...)
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

void ternel_main(void)
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
    teyboard_init();

    tlog("TraitOS v0.1.0 booted on x86_64\n");
    tlog("interrupts: IDT + PIC, PIT @100Hz, PS/2 keyboard\n");

    tprintf("===============================================\n");
    tprintf(" TraitOS v0.1.0 - RAM-resident, amnesic OS\n");
    tprintf("===============================================\n\n");

    tprintf(" arch      : x86_64\n");
    tprintf(" boot      : GRUB2 / Multiboot2 (BIOS + UEFI)\n");
    tprintf(" storage   : none - runs entirely from RAM\n\n");

    tprintf(" Type something and press Enter. Uptime logs to serial.\n");
    tprintf("   > ");

    __asm__ volatile("sti");

    for (;;) {
        __asm__ volatile("hlt");

        uint32_t now = timer_ticks();
        if (now / 100 != last_sec) {
            last_sec = now / 100;
            tlog("uptime: %us\n", last_sec);
        }

        int c;
        while ((c = teyboard_getchar()) >= 0) {
            if (c == '\n') {
                vga_putchar('\n');
                if (line_len > 0)
                    tlog("input: %s\n", line);
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
