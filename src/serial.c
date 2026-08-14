#include "serial.h"

#include <stdarg.h>
#include <stdint.h>

#define UART_DATA 0
#define UART_IER  1
#define UART_FCR  2
#define UART_LCR  3
#define UART_MCR  4
#define UART_LSR  5

#define LCR_8BIT   0x03
#define LCR_DLAB   0x80
#define LSR_THRE   0x20
#define FCR_ENABLE 0x01
#define FCR_CLEAR  0x06
#define FCR_TRIG_14 0xC0

static int com1_ready = 0;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

int serial_init(uint16_t port)
{
    outb(port + UART_IER, 0x00);         /* disable interrupts */
    outb(port + UART_LCR, LCR_DLAB);     /* DLAB on */
    outb(port + UART_DATA, 0x03);        /* divisor low  (38400 baud) */
    outb(port + UART_IER, 0x00);         /* divisor high */
    outb(port + UART_LCR, LCR_8BIT);     /* 8N1 */
    outb(port + UART_FCR, FCR_ENABLE | FCR_CLEAR | FCR_TRIG_14);
    outb(port + UART_MCR, 0x0B);         /* IRQs enabled, RTS/DSR set */
    if (port == COM1)
        com1_ready = 1;
    return 0;
}

void serial_putchar(uint16_t port, char c)
{
    while ((inb(port + UART_LSR) & LSR_THRE) == 0)
        ;
    outb(port + UART_DATA, (uint8_t)c);
}

void serial_puts(uint16_t port, const char *s)
{
    while (*s)
        serial_putchar(port, *s++);
}

static void u2s(uint32_t v, unsigned base, char *out)
{
    static const char digits[] = "0123456789abcdef";
    char tmp[16];
    int i = 0, n = 0;
    if (v == 0)
        tmp[i++] = '0';
    while (v) {
        tmp[i++] = digits[v % base];
        v /= base;
    }
    while (i > 0)
        out[n++] = tmp[--i];
    out[n] = '\0';
}

static void serial_vprintf(uint16_t port, const char *fmt, va_list ap)
{
    while (*fmt) {
        char c = *fmt++;
        if (c != '%') {
            serial_putchar(port, c);
            continue;
        }
        if (*fmt == '%') {
            fmt++;
            serial_putchar(port, '%');
            continue;
        }
        if (*fmt == 's') {
            fmt++;
            const char *s = va_arg(ap, const char *);
            serial_puts(port, s ? s : "(null)");
        } else if (*fmt == 'd') {
            fmt++;
            int v = va_arg(ap, int);
            char buf[16];
            if (v < 0) {
                serial_putchar(port, '-');
                u2s((uint32_t)(0 - (uint32_t)v), 10, buf);
            } else {
                u2s((uint32_t)v, 10, buf);
            }
            serial_puts(port, buf);
        } else if (*fmt == 'u') {
            fmt++;
            char buf[16];
            u2s(va_arg(ap, uint32_t), 10, buf);
            serial_puts(port, buf);
        } else if (*fmt == 'x') {
            fmt++;
            char buf[16];
            u2s(va_arg(ap, uint32_t), 16, buf);
            serial_puts(port, buf);
        } else if (*fmt == 'p') {
            fmt++;
            char buf[16];
            serial_puts(port, "0x");
            u2s((uint32_t)(uintptr_t)va_arg(ap, void *), 16, buf);
            serial_puts(port, buf);
        } else if (*fmt == 'c') {
            fmt++;
            serial_putchar(port, (char)va_arg(ap, int));
        } else {
            serial_putchar(port, '%');
        }
    }
}

void serial_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    serial_vprintf(COM1, fmt, ap);
    va_end(ap);
}

void tlog(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    serial_puts(COM1, "[traitos] ");
    serial_vprintf(COM1, fmt, ap);
    va_end(ap);
}

int serial_ready(void)
{
    return com1_ready;
}
