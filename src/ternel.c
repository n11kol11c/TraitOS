#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include "vga.h"
#include "serial.h"
#include "teyboard.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "tmalloc.h"
#include "tstring.h"
#include "tpmm.h"
#include "tvmm.h"

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

/* ---- command interpreter --------------------------------------------- */

static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_uptime(int argc, char **argv);
static void cmd_ver(int argc, char **argv);
static void cmd_info(int argc, char **argv);
static void cmd_alloc(int argc, char **argv);
static void cmd_paging(int argc, char **argv);
static void cmd_heap(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_die(int argc, char **argv);

static const struct {
    const char *name;
    void (*fn)(int argc, char **argv);
    const char *desc;
} commands[] = {
    { "help",   cmd_help,   "list available commands" },
    { "clear",  cmd_clear,  "clear the screen" },
    { "uptime", cmd_uptime, "seconds since boot" },
    { "ver",    cmd_ver,    "kernel version" },
    { "info",   cmd_info,   "system + memory info" },
    { "alloc",  cmd_alloc,  "allocate and free 8 physical pages" },
    { "paging", cmd_paging, "map a page at high vaddr, poke it, unmap" },
    { "heap",   cmd_heap,   "exercise the kernel heap (tmalloc/tfree)" },
    { "echo",   cmd_echo,   "print the rest of the line" },
    { "die",    cmd_die,    "divide by zero (panic demo)" },
};

#define NCOMMANDS (sizeof(commands) / sizeof(commands[0]))

static void cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tprintf(" commands:\n");
    for (size_t i = 0; i < NCOMMANDS; i++)
        tprintf("   %-8s  %s\n", commands[i].name, commands[i].desc);
}

static void cmd_clear(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    vga_clear();
}

static void cmd_uptime(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tprintf(" uptime: %us (%u ticks)\n", timer_ticks() / 100, timer_ticks());
}

static void cmd_ver(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tprintf(" TraitOS v0.4.0 (x86_64, Multiboot2)\n");
}

static void cmd_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tprintf(" arch      : x86_64\n");
    tprintf(" boot      : GRUB2 / Multiboot2 (BIOS + UEFI)\n");
    tprintf(" storage   : none - runs entirely from RAM\n");
    tprintf(" memory    : %u MiB total, %u MiB available (%u KiB)\n",
            (uint32_t)(tpmm_total_mem() >> 20),
            (uint32_t)(tpmm_available_mem() >> 20),
            (uint32_t)(tpmm_available_mem() >> 10) % 1024);
    tprintf(" frames    : %u free / %u used\n",
            tpmm_free_frames(), tpmm_used_frames());
    tprintf(" heap      : %u KiB mapped, %u KiB used, %u blocks\n",
            (uint32_t)(tmalloc_total() >> 10),
            (uint32_t)(tmalloc_used() >> 10), tmalloc_blocks());
}

static void cmd_alloc(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uintptr_t pages[8];
    tprintf(" allocating 8 frames:\n");
    for (int i = 0; i < 8; i++) {
        pages[i] = tpmm_alloc();
        if (pages[i])
            tprintf("   frame %d @ 0x%x\n", i, (uint32_t)pages[i]);
        else
            tprintf("   frame %d: FAILED\n", i);
    }
    for (int i = 0; i < 8; i++)
        if (pages[i])
            tpmm_free(pages[i]);
    tprintf(" freed them again (%u frames free)\n", tpmm_free_frames());
}

static void cmd_paging(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uintptr_t frame = tpmm_alloc();
    if (!frame) {
        tprintf(" out of memory\n");
        return;
    }
    uintptr_t virt = 0xFFFF800000000000ULL;
    tprintf(" frame @ 0x%x, mapping at 0xffff800000000000\n", (uint32_t)frame);
    if (vmm_map_page(virt, frame, VMM_PAGE_WRITE) != 0) {
        tprintf(" map failed\n");
        tpmm_free(frame);
        return;
    }
    volatile uint32_t *p = (volatile uint32_t *)virt;
    *p = 0x54524149;
    tprintf(" wrote 0x54524149, read back 0x%x\n", *p);
    vmm_unmap_page(virt);
    tpmm_free(frame);
    tprintf(" unmapped and freed\n");
}

static void cmd_heap(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tprintf(" heap: %u KiB mapped, %u KiB used, %u blocks\n",
            (uint32_t)(tmalloc_total() >> 10),
            (uint32_t)(tmalloc_used() >> 10), tmalloc_blocks());
    void *a = tmalloc(64);
    void *b = tmalloc(1024);
    void *c = tmalloc(48);
    tprintf(" tmalloc(64)=%p tmalloc(1024)=%p tmalloc(48)=%p\n", a, b, c);
    tprintf(" used: %u KiB, %u blocks\n",
            (uint32_t)(tmalloc_used() >> 10), tmalloc_blocks());
    tfree(b);
    tfree(a);
    tfree(c);
    tprintf(" after free: %u KiB used, %u blocks\n",
            (uint32_t)(tmalloc_used() >> 10), tmalloc_blocks());
}

static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            tprintf(" ");
        tprintf("%s", argv[i]);
    }
    tprintf("\n");
}

static void cmd_die(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tprintf(" dividing by zero...\n");
    volatile int x = 1;
    tprintf(" result: %d\n", 5 / (x - 1));
}

static int tokenize(char *line, char **argv, int max)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ')
            p++;
        if (*p)
            *p++ = '\0';
    }
    return argc;
}

static void run_command(char *line)
{
    char *argv[8];
    int argc = tokenize(line, argv, 8);
    if (argc == 0)
        return;
    for (size_t i = 0; i < NCOMMANDS; i++) {
        if (tstrcmp(argv[0], commands[i].name) == 0) {
            commands[i].fn(argc, argv);
            return;
        }
    }
    tprintf(" unknown command '%s' (try 'help')\n", argv[0]);
}

/* ---- entry point ------------------------------------------------------- */

void ternel_main(uintptr_t mbi)
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
    tpmm_init(mbi);
    vmm_init();

    tlog("TraitOS v0.4.0 booted on x86_64\n");
    tlog("memory map: %u MiB available (%u frames)\n",
         (uint32_t)(tpmm_available_mem() >> 20), tpmm_free_frames());

    tprintf("===============================================\n");
    tprintf(" TraitOS v0.4.0 - RAM-resident, amnesic OS\n");
    tprintf("===============================================\n\n");

    tprintf(" Type 'help' for a list of commands.\n");
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
                if (line_len > 0) {
                    tlog("cmd: %s\n", line);
                    run_command(line);
                }
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
