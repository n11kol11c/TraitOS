#include "tstring.h"

#include <stdarg.h>

size_t tstrlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

int tstrcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int tstrncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int d = (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        if (d != 0 || a[i] == '\0')
            return d;
    }
    return 0;
}

char *tstrcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++))
        ;
    return dst;
}

char *tstrncpy(char *dst, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';
    return dst;
}

char *tstrcat(char *dst, const char *src)
{
    char *d = dst + tstrlen(dst);
    while ((*d++ = *src++))
        ;
    return dst;
}

int tstartswith(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s != *prefix)
            return 0;
        s++;
        prefix++;
    }
    return 1;
}

char *tstrchr(const char *s, int c)
{
    while (*s) {
        if (*s == c)
            return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}

void *tmemset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    for (size_t i = 0; i < n; i++)
        p[i] = (unsigned char)c;
    return s;
}

void *tmemcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

void *tmemmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dst;
}

int tmemcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    for (size_t i = 0; i < n; i++) {
        int d = (int)x[i] - (int)y[i];
        if (d != 0)
            return d;
    }
    return 0;
}

void titoa(uint32_t val, char *buf, int base)
{
    static const char digits[] = "0123456789abcdef";
    char tmp[16];
    int i = 0, n = 0;
    if (val == 0)
        tmp[i++] = '0';
    while (val) {
        tmp[i++] = digits[val % (uint32_t)base];
        val /= (uint32_t)base;
    }
    while (i > 0)
        buf[n++] = tmp[--i];
    buf[n] = '\0';
}

static void buf_put(char *buf, size_t cap, size_t *i, char c)
{
    if (*i + 1 < cap)
        buf[*i] = c;
    (*i)++;
}

static void buf_putu(char *buf, size_t cap, size_t *i, uint32_t u)
{
    char tmp[12];
    int t = 0;
    if (u == 0)
        tmp[t++] = '0';
    while (u) {
        tmp[t++] = '0' + (u % 10);
        u /= 10;
    }
    while (t)
        buf_put(buf, cap, i, tmp[--t]);
}

static void buf_putx(char *buf, size_t cap, size_t *i, uint32_t u)
{
    static const char digits[] = "0123456789abcdef";
    char tmp[10];
    int t = 0;
    if (u == 0)
        tmp[t++] = '0';
    while (u) {
        tmp[t++] = digits[u & 0xF];
        u >>= 4;
    }
    while (t)
        buf_put(buf, cap, i, tmp[--t]);
}

/* Minimal snprintf: %s %d %u %x %p %c %%. No width/flags/precision. */
int tsprintf(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    size_t i = 0;
    while (*fmt) {
        char c = *fmt++;
        if (c != '%') {
            buf_put(buf, cap, &i, c);
            continue;
        }
        switch (*fmt) {
        case '%':
            fmt++;
            buf_put(buf, cap, &i, '%');
            break;
        case 's': {
            fmt++;
            const char *s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            while (*s)
                buf_put(buf, cap, &i, *s++);
            break;
        }
        case 'd': {
            fmt++;
            int v = va_arg(ap, int);
            if (v < 0) {
                buf_put(buf, cap, &i, '-');
                v = (int)(0u - (uint32_t)v);
            }
            buf_putu(buf, cap, &i, (uint32_t)v);
            break;
        }
        case 'u':
            fmt++;
            buf_putu(buf, cap, &i, va_arg(ap, uint32_t));
            break;
        case 'x':
            fmt++;
            buf_putx(buf, cap, &i, va_arg(ap, uint32_t));
            break;
        case 'p': {
            fmt++;
            uintptr_t p = (uintptr_t)va_arg(ap, void *);
            const char *pre = "0x";
            while (*pre)
                buf_put(buf, cap, &i, *pre++);
            buf_putx(buf, cap, &i, (uint32_t)p);
            break;
        }
        case 'c':
            fmt++;
            buf_put(buf, cap, &i, (char)va_arg(ap, int));
            break;
        default:
            buf_put(buf, cap, &i, '%');
            break;
        }
    }

    if (i < cap)
        buf[i] = '\0';
    else if (cap > 0)
        buf[cap - 1] = '\0';

    va_end(ap);
    return (int)i;
}
