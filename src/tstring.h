#ifndef TSTRING_H
#define TSTRING_H

#include <stddef.h>
#include <stdint.h>

size_t tstrlen(const char *s);
int    tstrcmp(const char *a, const char *b);
int    tstrncmp(const char *a, const char *b, size_t n);
char  *tstrcpy(char *dst, const char *src);
char  *tstrncpy(char *dst, const char *src, size_t n);
char  *tstrcat(char *dst, const char *src);
int    tstartswith(const char *s, const char *prefix);
char  *tstrchr(const char *s, int c);

void  *tmemset(void *s, int c, size_t n);
void  *tmemcpy(void *dst, const void *src, size_t n);
void  *tmemmove(void *dst, const void *src, size_t n);
int    tmemcmp(const void *a, const void *b, size_t n);

void   titoa(uint32_t val, char *buf, int base);
int    tsprintf(char *buf, size_t n, const char *fmt, ...);

#endif
