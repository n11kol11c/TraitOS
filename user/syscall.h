#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

/* User-mode syscall ABI (int 0x80): rax = number, rdi/rsi/rdx = args. */
#include <stdint.h>

static inline long sys_write(int fd, const void *buf, int len)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(1), "D"(fd), "S"(buf), "d"(len)
        : "rcx", "r11", "memory");
    return ret;
}

static inline void sys_exit(int code)
{
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(2), "D"(code)
        : "memory");
    __builtin_unreachable();
}

static inline int sys_getpid(void)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(3)
        : "rcx", "r11");
    return (int)ret;
}

static inline void sys_yield(void)
{
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(4)
        : "rcx", "r11", "memory");
}

static inline void sys_sleep(int ticks)
{
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(5), "D"(ticks)
        : "rcx", "r11", "memory");
}

static inline int sys_read(int fd, void *buf, int len)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(13), "D"(fd), "S"(buf), "d"(len)
        : "rcx", "r11", "memory");
    return (int)ret;
}

static inline int sys_getticks(void)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(12)
        : "rcx", "r11");
    return (int)ret;
}

static inline int sys_mb_init(void)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(6)
        : "rcx", "r11");
    return (int)ret;
}

static inline int sys_mb_send(int id, uint32_t *msg)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(7), "D"(id), "S"(msg)
        : "rcx", "r11", "memory");
    return (int)ret;
}

static inline int sys_mb_recv(int id, uint32_t *msg)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(8), "D"(id), "S"(msg)
        : "rcx", "r11", "memory");
    return (int)ret;
}

static inline int sys_mutex_init(void)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(9)
        : "rcx", "r11");
    return (int)ret;
}

static inline int sys_mutex_lock(int id)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(10), "D"(id)
        : "rcx", "r11", "memory");
    return (int)ret;
}

static inline int sys_mutex_unlock(int id)
{
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(11), "D"(id)
        : "rcx", "r11", "memory");
    return (int)ret;
}

#endif
