#ifndef TSYS_H
#define TSYS_H

/* int 0x80 syscall ABI: rax = number, rdi/rsi/rdx/r10/r8 = args, rax =
 * result (negative errno-style value on failure). */
enum {
    TSYS_WRITE      = 1,
    TSYS_EXIT       = 2,
    TSYS_GETPID     = 3,
    TSYS_YIELD      = 4,
    TSYS_SLEEP      = 5,
    /* M8 C1: IPC syscalls */
    TSYS_MB_INIT    = 6,
    TSYS_MB_SEND    = 7,
    TSYS_MB_RECV    = 8,
    TSYS_MUTEX_INIT = 9,
    TSYS_MUTEX_LOCK = 10,
    TSYS_MUTEX_UNLOCK = 11,
    TSYS_GETTICKS   = 12,
};

#endif
