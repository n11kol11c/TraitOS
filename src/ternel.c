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
#include "tsec.h"
#include "ttask.h"
#include "tfs.h"
#include "tsh.h"

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

/* ---- line editor with history ---------------------------------------- */

static int  hist_pos = 0;
static int  prompt_row = 0;

static void print_prompt(void)
{
    vga_puts("   > ");
    prompt_row = vga_get_row();   /* prompt always starts at col 0 */
}

static void redraw_line(const char *line, int len, int cursor)
{
    int rows = (len + 6 + VGA_WIDTH - 1) / VGA_WIDTH;   /* prompt + line */
    if (rows < 1)
        rows = 1;
    for (int r = 0; r < rows; r++) {
        vga_set_cursor(0, prompt_row + r);
        for (int c = 0; c < VGA_WIDTH; c++)
            vga_putchar(' ');
    }
    vga_set_cursor(0, prompt_row);
    vga_puts("   > ");
    vga_puts(line);
    vga_set_cursor((6 + cursor) % VGA_WIDTH,
                   prompt_row + (6 + cursor) / VGA_WIDTH);
}

static void place_cursor(int cursor)
{
    vga_set_cursor((6 + cursor) % VGA_WIDTH,
                   prompt_row + (6 + cursor) / VGA_WIDTH);
}

static void cmd_hist(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (tsh_hist_count() == 0) {
        tprintf(" (empty)\n");
        return;
    }
    for (int i = 0; i < tsh_hist_count(); i++)
        tprintf(" %2d  %s\n", i, tsh_hist_get(i));
}

/* ---- environment ------------------------------------------------------ */

static void print_env(const char *key, const char *val)
{
    tprintf(" %s=%s\n", key, val);
}

static void cmd_env(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tsh_env_list(print_env);
}

static void cmd_set(int argc, char **argv)
{
    if (argc < 2) {
        tprintf(" usage: set KEY=VALUE\n");
        return;
    }
    char *eq = tstrchr(argv[1], '=');
    if (!eq) {
        tprintf(" set: expected KEY=VALUE\n");
        return;
    }
    *eq = '\0';
    if (tsh_env_set(argv[1], eq + 1) != 0)
        tprintf(" set: environment table full\n");
}

static void cmd_whoami(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const char *user = tsh_env_get("USER");
    tprintf(" %s\n", user ? user : "root");
}

static void cmd_halt(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tprintf(" scrubbing %u free frames...\n", (uint32_t)sec_scrub_ram());
    tprintf(" halting\n");
    __asm__ volatile("cli");
    for (;;)
        __asm__ volatile("hlt");
}

static void cmd_reboot(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tprintf(" scrubbing %u free frames...\n", (uint32_t)sec_scrub_ram());
    tprintf(" rebooting...\n");
    __asm__ volatile("cli");
    /* 8042 keyboard-controller system reset pulse */
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
    for (;;)
        __asm__ volatile("hlt");
}

static void cmd_sec(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uint64_t efer = sec_read_efer();
    uint64_t cr4 = sec_read_cr4();
    uint64_t cr0 = sec_read_cr0();

    tprintf(" EFER   : 0x%lx%s%s%s\n", (unsigned long)efer,
            (efer & (1ull << 11)) ? " NXE" : "",
            (efer & (1ull << 8)) ? " LME" : "",
            (efer & (1ull << 9)) ? " LMA" : "");
    tprintf(" CR4    : 0x%lx%s%s%s\n", (unsigned long)cr4,
            (cr4 & (1ull << 5)) ? " PAE" : "",
            (cr4 & (1ull << 20)) ? " SMEP" : "",
            (cr4 & (1ull << 21)) ? " SMAP" : "");
    tprintf(" CR0    : 0x%lx%s%s\n", (unsigned long)cr0,
            (cr0 & (1ull << 0)) ? " PE" : "",
            (cr0 & (1ull << 31)) ? " PG" : "");
    uintptr_t base = sec_kernel_phys_base();
    tprintf(" kaslr  : %s (image @ 0x%lx)\n", base ? "on" : "off",
            (unsigned long)(base ? base : 0x100000));
    tprintf(" W^X    : %s\n", sec_wx_enforced() ? "enforced" : "VIOLATED");
    tprintf(" NX     : %s\n", sec_nx_enforced() ? "enforced" : "VIOLATED");
    tprintf(" stack  : %s\n",
            sec_stack_guard_enabled() ? "guarded" : "unguarded");
    tprintf(" heap   : mapped non-executable\n");
}

static void cmd_scrub(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uint32_t n = sec_scrub_ram();
    tprintf(" zeroed %u free frames (%u KiB)\n", n, n / 4);
}

/* Demo task: spin forever, logging progress and yielding to the scheduler. */
static void demo_counter(void *arg)
{
    (void)arg;
    uint64_t n = 0;
    for (;;) {
        n++;
        if ((n & 0xFFFFF) == 0)
            tlog("task %s: %u\n", current_task->name, (uint32_t)n);
        if ((n & 0x3FFF) == 0)
            ttask_yield();
    }
}

static void cmd_tasks(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tprintf(" id  name      state    prio  ticks\n");
    for (int i = 0; i < TTASK_MAX_TASKS; i++) {
        ttask_t *t = ttask_at(i);
        if (!t || t->state == TTASK_FREE)
            continue;
        const char *st = t->state == TTASK_READY    ? "ready"
                         : t->state == TTASK_RUNNING ? "run"
                         : t->state == TTASK_EXITED  ? "done"
                                                     : "?";
        tprintf(" %-3d %-9s %-8s %-5u %-6u%s\n", i, t->name, st,
                t->priority, t->ticks,
                t == current_task ? "  <=" : "");
    }
}

static void cmd_spawn(int argc, char **argv)
{
    if (argc < 2) {
        tprintf(" usage: spawn <name>\n");
        return;
    }
    ttask_t *t = ttask_create(argv[1], demo_counter, 0);
    tprintf(" %s: %s\n", argv[1], t ? "spawned" : "FAILED (table full)");
}

static void cmd_burst(int argc, char **argv)
{
    unsigned n = 4;
    if (argc > 1) {
        n = 0;
        for (char *p = argv[1]; *p >= '0' && *p <= '9'; p++)
            n = n * 10 + (unsigned)(*p - '0');
    }
    if (n > 16)
        n = 16;
    int made = 0;
    for (unsigned i = 0; i < n; i++) {
        char name[TTASK_NAME_LEN];
        tsprintf(name, sizeof name, "d%u", i);
        if (ttask_create(name, demo_counter, 0))
            made++;
    }
    tprintf(" spawned %d task%s\n", made, made == 1 ? "" : "s");
}

static void cmd_yield(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tprintf(" yielding...\n");
    ttask_yield();
    tprintf(" back in shell\n");
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
static void cmd_aspace(int argc, char **argv);
static void cmd_hist(int argc, char **argv);
static void cmd_env(int argc, char **argv);
static void cmd_set(int argc, char **argv);
static void cmd_whoami(int argc, char **argv);
static void cmd_halt(int argc, char **argv);
static void cmd_reboot(int argc, char **argv);
static void cmd_sec(int argc, char **argv);
static void cmd_scrub(int argc, char **argv);
static void cmd_tasks(int argc, char **argv);
static void cmd_spawn(int argc, char **argv);
static void cmd_burst(int argc, char **argv);
static void cmd_yield(int argc, char **argv);
static void cmd_ls(int argc, char **argv);
static void cmd_cat(int argc, char **argv);
static void cmd_mkdir(int argc, char **argv);
static void cmd_touch(int argc, char **argv);
static void cmd_rm(int argc, char **argv);
static void cmd_write(int argc, char **argv);
static void cmd_mount(int argc, char **argv);

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
    { "aspace", cmd_aspace, "demo per-process address spaces (page tables)" },
    { "hist",   cmd_hist,   "show command history (up/down arrows recall)" },
    { "env",    cmd_env,    "show environment variables" },
    { "set",    cmd_set,    "set an environment variable (KEY=VALUE)" },
    { "whoami", cmd_whoami, "print the current user (from $USER)" },
    { "halt",   cmd_halt,   "halt the CPU (power-off not implemented)" },
    { "reboot", cmd_reboot, "reboot via the keyboard controller (8042)" },
    { "sec",    cmd_sec,    "show + verify CPU hardening (NX, W^X, SMEP/SMAP)" },
    { "scrub",  cmd_scrub,  "zero every free physical frame (amnesia)" },
    { "tasks",  cmd_tasks,  "list scheduler tasks" },
    { "spawn",  cmd_spawn,  "spawn a demo task" },
    { "burst",  cmd_burst,  "spawn N demo tasks" },
    { "yield",  cmd_yield,  "yield the CPU to another task" },
    { "ls",     cmd_ls,     "list a directory (default /)" },
    { "cat",    cmd_cat,    "print a file (ramfs, procfs, sysfs)" },
    { "mkdir",  cmd_mkdir,  "create a directory" },
    { "touch",  cmd_touch,  "create an empty file" },
    { "rm",     cmd_rm,     "remove a file or directory" },
    { "write",  cmd_write,  "write text into a file" },
    { "mount",  cmd_mount,  "list mounted filesystems" },
};

#define NCOMMANDS (sizeof(commands) / sizeof(commands[0]))

static void run_command(char *line)
{
    char *argv[TSH_ARGV_MAX];
    int argc = tsh_tokenize(line, argv, TSH_ARGV_MAX);
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
    tprintf(" TraitOS v0.8.0 (x86_64, Multiboot2)\n");
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
    uint64_t cr4 = sec_read_cr4();
    tprintf(" sec      : NX %s, W^X %s, SMEP/SMAP %s, kaslr %s\n",
            sec_nx_enforced() ? "on" : "off",
            sec_wx_enforced() ? "on" : "off",
            (cr4 & ((1ull << 20) | (1ull << 21))) ==
                    ((1ull << 20) | (1ull << 21))
                ? "on"
                : "off",
            sec_kernel_phys_base() ? "on" : "off");
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
    if (vmm_map_page(virt, frame, VMM_PAGE_WRITE | VMM_PAGE_NX) != 0) {
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

static void cmd_aspace(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uintptr_t addr = 0x0000008000000000ULL;   /* 512 GiB: PML4 slot 1 */

    vmm_aspace_t *kernel = vmm_aspace_current();

    vmm_aspace_t *a = vmm_aspace_create();
    vmm_aspace_t *b = vmm_aspace_create();
    if (!a || !b) {
        tprintf(" out of memory\n");
        return;
    }

    uintptr_t pa = tpmm_alloc();
    uintptr_t pb = tpmm_alloc();
    if (!pa || !pb) {
        tprintf(" out of frames\n");
        if (pa)
            tpmm_free(pa);
        if (pb)
            tpmm_free(pb);
        vmm_aspace_destroy(a);
        vmm_aspace_destroy(b);
        return;
    }

    vmm_aspace_map(a, addr, pa, VMM_PAGE_WRITE | VMM_PAGE_USER | VMM_PAGE_NX);
    vmm_aspace_map(b, addr, pb, VMM_PAGE_WRITE | VMM_PAGE_USER | VMM_PAGE_NX);
    tprintf(" 2 address spaces, same virtual page mapped in both\n");

    vmm_aspace_switch(a);
    *(volatile uint32_t *)addr = 0x11111111;
    tprintf("  in space A: wrote 0x11111111, read back 0x%x\n",
            *(volatile uint32_t *)addr);

    vmm_aspace_switch(b);
    *(volatile uint32_t *)addr = 0x22222222;
    tprintf("  in space B: wrote 0x22222222, read back 0x%x\n",
            *(volatile uint32_t *)addr);

    vmm_aspace_switch(a);
    tprintf("  in space A: still 0x%x (per-process isolation works)\n",
            *(volatile uint32_t *)addr);

    vmm_aspace_switch(kernel);
    vmm_aspace_unmap(a, addr);
    vmm_aspace_unmap(b, addr);
    tpmm_free(pa);
    tpmm_free(pb);
    vmm_aspace_destroy(a);
    vmm_aspace_destroy(b);
    tprintf(" spaces torn down, frames returned\n");
}

static void print_node(tfs_node_t *node)
{
    tprintf(" %s %u  %s%s\n", node->type == TFS_DIR ? "drw-" : "-rw-",
            (uint32_t)node->size, node->name,
            node->type == TFS_DIR ? "/" : "");
}

static void cmd_ls(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/";
    tfs_node_t *node = tfs_lookup(path);
    if (!node) {
        tprintf(" ls: '%s': not found\n", path);
        return;
    }
    if (node->type == TFS_DIR)
        tfs_list(node, print_node);
    else
        print_node(node);
}

static void cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        tprintf(" usage: cat <path>\n");
        return;
    }
    tfs_node_t *node = tfs_lookup(argv[1]);
    if (!node) {
        tprintf(" cat: '%s': not found\n", argv[1]);
        return;
    }
    if (node->type == TFS_DIR) {
        tprintf(" cat: '%s' is a directory\n", argv[1]);
        return;
    }
    if (node->gen) {
        char buf[256];
        node->gen(node, buf, sizeof buf);
        for (uint32_t i = 0; i < (uint32_t)node->size; i++)
            vga_putchar(buf[i]);
    } else if (node->data) {
        for (uint32_t i = 0; i < (uint32_t)node->size; i++)
            vga_putchar(node->data[i]);
    }
}

static void cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) {
        tprintf(" usage: mkdir <path>\n");
        return;
    }
    tprintf(tfs_mkdir(argv[1]) ? " ok\n" : " mkdir: '%s': failed or exists\n",
            argv[1]);
}

static void cmd_touch(int argc, char **argv)
{
    if (argc < 2) {
        tprintf(" usage: touch <path>\n");
        return;
    }
    tprintf(tfs_touch(argv[1]) ? " ok\n" : " touch: '%s': failed or exists\n",
            argv[1]);
}

static void cmd_rm(int argc, char **argv)
{
    if (argc < 2) {
        tprintf(" usage: rm <path>\n");
        return;
    }
    tfs_node_t *node = tfs_lookup(argv[1]);
    if (!node || tfs_rm(node) != 0)
        tprintf(" rm: '%s': failed\n", argv[1]);
    else
        tprintf(" ok\n");
}

static void cmd_write(int argc, char **argv)
{
    if (argc < 3) {
        tprintf(" usage: write <path> <text>\n");
        return;
    }
    tfs_node_t *node = tfs_lookup(argv[1]);
    if (!node) {
        tprintf(" write: '%s': not found\n", argv[1]);
        return;
    }
    char text[128];
    size_t len = 0;
    for (int i = 2; i < argc && len < sizeof text - 1; i++) {
        if (i > 2 && len < sizeof text - 1)
            text[len++] = ' ';
        size_t l = tstrlen(argv[i]);
        if (len + l > sizeof text - 1)
            l = sizeof text - 1 - len;
        tmemcpy(text + len, argv[i], l);
        len += l;
    }
    text[len] = '\0';
    if (tfs_write(node, text, len) == 0)
        tprintf(" ok (%u bytes)\n", (uint32_t)len);
    else
        tprintf(" write: '%s' is not writable\n", argv[1]);
}

static void print_mount(const char *name, tfs_node_t *node)
{
    tprintf("  %s  ->  %s\n", name, node->name);
}

static void cmd_mount(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tfs_mount_list(print_mount);
}

/* ---- command interpreter --------------------------------------------- */

/* ---- entry point ------------------------------------------------------- */

void ternel_main(uintptr_t mbi)
{
    char line[128];
    int line_len = 0;
    int cursor = 0;
    uint32_t last_sec = 0;

    line[0] = '\0';

    vga_init();
    serial_init(COM1);
    gdt_init();
    idt_init();
    timer_init(100);
    teyboard_init();
    tpmm_init(mbi);
    sec_kaslr_relocate(mbi);
    vmm_init();
    tfs_init();
    tfs_procfs_init();
    tfs_sysfs_init();
    tfs_load_initrd(mbi);
    tsh_env_seed();
    sec_harden();
    ttask_init();

    tlog("TraitOS v0.8.0 booted on x86_64\n");
    tlog("memory map: %u MiB available (%u frames)\n",
         (uint32_t)(tpmm_available_mem() >> 20), tpmm_free_frames());
    tlog("security: NX %s, W^X %s, SMEP+SMAP %s, KASLR %s, stack guard %s\n",
         sec_nx_enforced() ? "on" : "off",
         sec_wx_enforced() ? "on" : "off",
         (sec_read_cr4() & ((1ull << 20) | (1ull << 21))) ==
                 ((1ull << 20) | (1ull << 21))
             ? "on"
             : "off",
         sec_kernel_phys_base() ? "on" : "off",
         sec_stack_guard_enabled() ? "on" : "off");
    tlog("scheduler: preemptive round-robin @ %u Hz, %d tasks\n",
         (uint32_t)timer_hz(), ttask_count());

    tprintf("===============================================\n");
    tprintf(" TraitOS v0.8.0 - RAM-resident, amnesic OS\n");
    tprintf("===============================================\n\n");

    tprintf(" Type 'help' for a list of commands.\n");
    print_prompt();

    __asm__ volatile("sti");

    for (;;) {
        __asm__ volatile("hlt");

        uint32_t now = timer_ticks();
        if (now / 100 != last_sec) {
            last_sec = now / 100;
            tlog("uptime: %us\n", last_sec);
        }

        int c;
        while ((c = teyboard_getchar()) != KEY_NONE) {
            if (c == '\n') {
                vga_putchar('\n');
                if (line_len > 0) {
                    char expanded[TSH_HIST_LEN];
                    int hx = tsh_hist_expand(line, expanded,
                                             sizeof expanded);
                    if (hx < 0) {
                        tprintf(" history: no such entry\n");
                    } else {
                        if (hx > 0) {
                            tstrncpy(line, expanded, 128);
                            line_len = (int)tstrlen(line);
                        }
                        tsh_hist_push(line);
                        hist_pos = tsh_hist_count();
                        tlog("cmd: %s\n", line);
                        run_command(line);
                    }
                }
                line_len = 0;
                cursor = 0;
                line[0] = '\0';
                print_prompt();
            } else if (c == '\b') {
                if (cursor > 0) {
                    tmemmove(line + cursor - 1, line + cursor,
                             (size_t)(line_len - cursor) + 1);
                    line_len--;
                    line[line_len] = '\0';
                    cursor--;
                    hist_pos = tsh_hist_count();
                    redraw_line(line, line_len, cursor);
                }
            } else if (c == KEY_DEL) {
                if (cursor < line_len) {
                    tmemmove(line + cursor, line + cursor + 1,
                             (size_t)(line_len - cursor));
                    line_len--;
                    line[line_len] = '\0';
                    hist_pos = tsh_hist_count();
                    redraw_line(line, line_len, cursor);
                }
            } else if (c == KEY_LEFT) {
                if (cursor > 0) {
                    cursor--;
                    place_cursor(cursor);
                }
            } else if (c == KEY_RIGHT) {
                if (cursor < line_len) {
                    cursor++;
                    place_cursor(cursor);
                }
            } else if (c == KEY_HOME) {
                cursor = 0;
                place_cursor(cursor);
            } else if (c == KEY_END) {
                cursor = line_len;
                place_cursor(cursor);
            } else if (c == KEY_UP) {
                if (hist_pos > 0) {
                    hist_pos--;
                    tstrncpy(line, tsh_hist_get(hist_pos), 128);
                    line_len = (int)tstrlen(line);
                    cursor = line_len;
                    redraw_line(line, line_len, cursor);
                }
            } else if (c == KEY_DOWN) {
                if (hist_pos < tsh_hist_count()) {
                    hist_pos++;
                    if (hist_pos >= tsh_hist_count()) {
                        line[0] = '\0';
                        line_len = 0;
                    } else {
                        tstrncpy(line, tsh_hist_get(hist_pos), 128);
                        line_len = (int)tstrlen(line);
                    }
                    cursor = line_len;
                    redraw_line(line, line_len, cursor);
                }
            } else if (c >= 32 && line_len < 127) {
                hist_pos = tsh_hist_count();
                if (cursor == line_len) {
                    line[line_len++] = (char)c;
                    line[line_len] = '\0';
                    cursor++;
                    vga_putchar((char)c);
                } else {
                    tmemmove(line + cursor + 1, line + cursor,
                             (size_t)(line_len - cursor) + 1);
                    line[cursor] = (char)c;
                    line_len++;
                    line[line_len] = '\0';
                    cursor++;
                    redraw_line(line, line_len, cursor);
                }
            }
        }
    }
}
