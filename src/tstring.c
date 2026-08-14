#include "tstring.h"

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
