#include "tpmm.h"

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE       4096
#define PMM_MAX_ADDR    0x100000000ULL          /* 4 GiB ceiling */
#define PMM_MAX_FRAMES  (PMM_MAX_ADDR / PAGE_SIZE)
#define PMM_BITMAP_WORDS (PMM_MAX_FRAMES / 32)

/* One bit per 4 KiB physical frame; 1 = used, 0 = free. */
static uint32_t bitmap[PMM_BITMAP_WORDS];
static uint32_t total_frames = 0;
static uint32_t free_frames = 0;
static uint32_t first_free_hint = 0;

extern char _kernel_phys_start[];
extern char _kernel_phys_end[];

/* Multiboot2 memory map tag */
#define MB2_TAG_MMAP     6
#define MB2_MMAP_AVAIL   1

struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_mmap_entry {
    uint64_t base;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
};

static inline void bit_set(uint32_t frame)
{
    if (frame < PMM_MAX_FRAMES)
        bitmap[frame / 32] |= (1u << (frame % 32));
}

static inline void bit_clear(uint32_t frame)
{
    if (frame < PMM_MAX_FRAMES)
        bitmap[frame / 32] &= ~(1u << (frame % 32));
}

static inline int bit_test(uint32_t frame)
{
    return (bitmap[frame / 32] >> (frame % 32)) & 1u;
}

static void reserve_frame(uintptr_t phys)
{
    uint32_t frame = (uint32_t)(phys / PAGE_SIZE);
    if (frame >= total_frames)
        return;
    if (!bit_test(frame))
        free_frames--;
    bit_set(frame);
}

static void reserve_region(uintptr_t base, size_t size)
{
    for (uintptr_t a = base & ~(uintptr_t)(PAGE_SIZE - 1);
         a < base + size; a += PAGE_SIZE)
        reserve_frame(a);
}

void tpmm_init(uintptr_t mbi)
{
    uintptr_t p = mbi + 8;                   /* skip total_size + reserved */
    uintptr_t end = mbi + *(uint32_t *)mbi;

    /* start with every frame used, then free what the BIOS says is RAM */
    for (uint32_t i = 0; i < PMM_BITMAP_WORDS; i++)
        bitmap[i] = 0xFFFFFFFF;
    total_frames = 0;
    free_frames = 0;
    first_free_hint = 0;

    while (p + 8 <= end) {
        struct mb2_tag *tag = (struct mb2_tag *)p;
        if (tag->type == 0)
            break;
        if (tag->type == MB2_TAG_MMAP) {
            uint32_t entry_size = *(uint32_t *)(p + 8);
            struct mb2_mmap_entry *e =
                (struct mb2_mmap_entry *)(p + 16);
            size_t n = (tag->size - 16) / entry_size;
            for (size_t i = 0; i < n; i++) {
                if (e->type == MB2_MMAP_AVAIL) {
                    uintptr_t start = (uintptr_t)e->base;
                    uintptr_t stop = start + (uintptr_t)e->len;
                    if (stop > PMM_MAX_ADDR)
                        stop = (uintptr_t)PMM_MAX_ADDR;
                    uint32_t f_start = (uint32_t)((start + PAGE_SIZE - 1) / PAGE_SIZE);
                    uint32_t f_end = (uint32_t)(stop / PAGE_SIZE);
                    for (uint32_t f = f_start; f < f_end; f++) {
                        if (bit_test(f)) {
                            bit_clear(f);
                            free_frames++;
                        }
                        if (f >= total_frames)
                            total_frames = f + 1;
                    }
                }
                e = (struct mb2_mmap_entry *)((uintptr_t)e + entry_size);
            }
        }
        p += (tag->size + 7) & ~(uintptr_t)7;
    }

    /* keep the 1 MiB low region and the loaded kernel image out of the heap */
    reserve_region(0x00000000, 0x100000);
    reserve_region((uintptr_t)_kernel_phys_start,
                   (uintptr_t)_kernel_phys_end - (uintptr_t)_kernel_phys_start);
}

uintptr_t tpmm_alloc(void)
{
    for (uint32_t f = first_free_hint; f < total_frames; f++) {
        if (!bit_test(f)) {
            bit_set(f);
            free_frames--;
            first_free_hint = f + 1;
            return (uintptr_t)f * PAGE_SIZE;
        }
    }
    for (uint32_t f = 0; f < first_free_hint && f < total_frames; f++) {
        if (!bit_test(f)) {
            bit_set(f);
            free_frames--;
            first_free_hint = f + 1;
            return (uintptr_t)f * PAGE_SIZE;
        }
    }
    return 0;
}

/* Low-memory (< 1 GiB) single frame: guaranteed reachable via the
 * boot identity map and the higher-half physmap window. */
uintptr_t tpmm_alloc_low(void)
{
    for (uint32_t f = first_free_hint; f < 0x40000 && f < total_frames; f++) {
        if (!bit_test(f)) {
            bit_set(f);
            free_frames--;
            first_free_hint = f + 1;
            return (uintptr_t)f * PAGE_SIZE;
        }
    }
    for (uint32_t f = 0; f < first_free_hint && f < 0x40000; f++) {
        if (!bit_test(f)) {
            bit_set(f);
            free_frames--;
            first_free_hint = f + 1;
            return (uintptr_t)f * PAGE_SIZE;
        }
    }
    return 0;
}

/* Allocate `pages` contiguous physical frames (for the heap). */
uintptr_t tpmm_alloc_contig(size_t pages)
{
    uint32_t run = 0;
    uint32_t start = 0;

    for (uint32_t f = first_free_hint; f < total_frames; f++) {
        if (!bit_test(f)) {
            if (run == 0)
                start = f;
            if (++run == pages)
                goto found;
        } else {
            run = 0;
        }
    }
    for (uint32_t f = 0; f < first_free_hint; f++) {
        if (!bit_test(f)) {
            if (run == 0)
                start = f;
            if (++run == pages)
                goto found;
        } else {
            run = 0;
        }
    }
    return 0;

found:
    for (uint32_t i = 0; i < pages; i++) {
        bit_set(start + i);
        free_frames--;
    }
    first_free_hint = start + pages;
    return (uintptr_t)start * PAGE_SIZE;
}

void tpmm_free(uintptr_t phys)
{
    uint32_t frame = (uint32_t)(phys / PAGE_SIZE);
    if (frame >= total_frames)
        return;
    if (bit_test(frame)) {
        bit_clear(frame);
        free_frames++;
        if (frame < first_free_hint)
            first_free_hint = frame;
    }
}

uint32_t tpmm_free_frames(void)
{
    return free_frames;
}

uint32_t tpmm_used_frames(void)
{
    return total_frames - free_frames;
}

uint64_t tpmm_total_mem(void)
{
    return (uint64_t)total_frames * PAGE_SIZE;
}

uint64_t tpmm_available_mem(void)
{
    return (uint64_t)free_frames * PAGE_SIZE;
}
