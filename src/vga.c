#include "vga.h"
#include "kstring.h"

#include <stddef.h>

static volatile uint16_t *const VGA_MEM = (uint16_t *)0xB8000;

static int terminal_row;
static int terminal_col;
static uint8_t terminal_color;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void update_cursor(void)
{
    uint16_t pos = (uint16_t)(terminal_row * VGA_WIDTH + terminal_col);
    outb(0x3D4, 14);
    outb(0x3D5, (uint8_t)(pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (uint8_t)pos);
}

void vga_init(void)
{
    vga_clear();
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_set_cursor(0, 0);
}

void vga_clear(void)
{
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEM[i] = (uint16_t)((terminal_color << 8) | ' ');
    terminal_row = 0;
    terminal_col = 0;
    update_cursor();
}

void vga_set_color(vga_color fg, vga_color bg)
{
    terminal_color = (uint8_t)((uint8_t)fg | ((uint8_t)bg << 4));
}

void vga_scroll(void)
{
    for (size_t r = 1; r < VGA_HEIGHT; r++)
        for (size_t c = 0; c < VGA_WIDTH; c++)
            VGA_MEM[(r - 1) * VGA_WIDTH + c] = VGA_MEM[r * VGA_WIDTH + c];
    for (size_t c = 0; c < VGA_WIDTH; c++)
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + c] =
            (uint16_t)((terminal_color << 8) | ' ');
}

void vga_putchar(char c)
{
    if (c == '\n') {
        terminal_col = 0;
        terminal_row++;
    } else if (c == '\r') {
        terminal_col = 0;
    } else if (c == '\t') {
        terminal_col = (terminal_col + 4) & ~3;
    } else if (c == '\b') {
        if (terminal_col > 0)
            terminal_col--;
        VGA_MEM[terminal_row * VGA_WIDTH + terminal_col] =
            (uint16_t)((terminal_color << 8) | ' ');
    } else {
        VGA_MEM[terminal_row * VGA_WIDTH + terminal_col] =
            (uint16_t)((terminal_color << 8) | (uint8_t)c);
        terminal_col++;
    }

    if (terminal_col >= VGA_WIDTH) {
        terminal_col = 0;
        terminal_row++;
    }
    if (terminal_row >= VGA_HEIGHT) {
        vga_scroll();
        terminal_row = VGA_HEIGHT - 1;
    }
    update_cursor();
}

void vga_puts(const char *str)
{
    while (*str)
        vga_putchar(*str++);
}

void vga_put_hex(uint32_t val)
{
    char buf[16];
    kitoa(val, buf, 16);
    vga_puts(buf);
}

void vga_put_dec(uint32_t val)
{
    char buf[16];
    kitoa(val, buf, 10);
    vga_puts(buf);
}

void vga_set_cursor(int x, int y)
{
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= VGA_WIDTH)
        x = VGA_WIDTH - 1;
    if (y >= VGA_HEIGHT)
        y = VGA_HEIGHT - 1;
    terminal_col = x;
    terminal_row = y;
    update_cursor();
}

int vga_get_col(void)
{
    return terminal_col;
}

int vga_get_row(void)
{
    return terminal_row;
}
