#include "tss.h"

#include "tstring.h"

/* 64-bit Task State Segment. The CPU switches to rsp0 (the current task's
 * kernel stack) on every ring-3 -> ring-0 transition: interrupts, exceptions
 * and the `int 0x80` syscall gate. */
typedef struct {
    uint32_t reserved1;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved2;
    uint64_t ist[7];
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

static tss_t tss;

void tss_init(void)
{
    tmemset(&tss, 0, sizeof tss);
    tss.iomap_base = sizeof tss;   /* no I/O permission bitmap */
}

void tss_set_rsp0(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}

uintptr_t tss_base(void)
{
    return (uintptr_t)&tss;
}

uint32_t tss_limit(void)
{
    return (uint32_t)(sizeof tss) - 1;
}
