#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define COM1  0x3F8
#define COM2  0x2F8
#define COM3  0x3E8
#define COM4  0x2E8

int  serial_init(uint16_t port);
void serial_putchar(uint16_t port, char c);
void serial_puts(uint16_t port, const char *s);
void serial_printf(const char *fmt, ...);
void klog(const char *fmt, ...);
int  serial_ready(void);

#endif
