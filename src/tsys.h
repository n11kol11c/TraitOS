#ifndef TSYS_H
#define TSYS_H

/* int 0x80 syscall ABI: rax = number, rdi/rsi/rdx/r10/r8 = args, rax =
 * result (negative errno-style value on failure). */
enum {
    TSYS_WRITE  = 1,
    TSYS_EXIT   = 2,
    TSYS_GETPID = 3,
    TSYS_YIELD  = 4,
    TSYS_SLEEP  = 5,
};

#endif
