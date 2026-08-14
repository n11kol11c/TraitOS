#include "gdt.h"

#include "tss.h"
#include "tstring.h"

#include <stdint.h>

/* Selector layout: null(0x00) / kcode(0x08) / kdata(0x10) / ucode(0x18) /
 * udata(0x20) / tss(0x28, 16-byte descriptor). */
#define GDT_KCODE 0x08
#define GDT_KDATA 0x10
#define GDT_UCODE 0x18
#define GDT_UDATA 0x20
#define GDT_TSS   0x28

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

static gdt_entry_t gdt[6];

#define ACCESS_CODE 0x9A   /* present, DPL 0, code, non-conforming, readable */
#define ACCESS_DATA 0x92   /* present, DPL 0, data, writable */
#define ACCESS_TSS  0x89   /* present, DPL 0, available 64-bit TSS */
#define DPL3         0x60
#define GRAN_LONG    0x20   /* 64-bit (L=1) code segment */

static void set_segment(uint16_t selector, uint8_t access, uint8_t gran)
{
    gdt_entry_t *e = &gdt[selector >> 3];
    e->limit_low = 0;
    e->base_low = 0;
    e->base_mid = 0;
    e->access = access;
    e->granularity = gran;
    e->base_high = 0;
    e->base_upper = 0;
    e->reserved = 0;
}

static void set_tss_descriptor(uint16_t selector, uintptr_t base,
                               uint32_t limit)
{
    gdt_entry_t *e = &gdt[selector >> 3];
    e->limit_low = limit & 0xFFFF;
    e->base_low = (uint16_t)(base & 0xFFFF);
    e->base_mid = (uint8_t)((base >> 16) & 0xFF);
    e->access = ACCESS_TSS;
    e->granularity = (uint8_t)((limit >> 16) & 0x0F);
    e->base_high = (uint8_t)((base >> 24) & 0xFF);
    e->base_upper = (uint32_t)(base >> 32);
    e->reserved = 0;
}

void gdt_init(void)
{
    tmemset(gdt, 0, sizeof gdt);

    set_segment(GDT_KCODE, ACCESS_CODE, GRAN_LONG);
    set_segment(GDT_KDATA, ACCESS_DATA, 0x00);
    set_segment(GDT_UCODE, ACCESS_CODE | DPL3, GRAN_LONG);
    set_segment(GDT_UDATA, ACCESS_DATA | DPL3, 0x00);

    tss_init();
    set_tss_descriptor(GDT_TSS, tss_base(), tss_limit());

    gdt_ptr_t ptr;
    ptr.limit = sizeof gdt - 1;
    ptr.base = (uint64_t)&gdt[0];
    __asm__ volatile("lgdt %0" : : "m"(ptr));
    gdt_reload();
    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS));
}
