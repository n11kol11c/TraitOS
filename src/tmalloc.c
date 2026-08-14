#include "tmalloc.h"
#include "tpmm.h"

#include <stdint.h>

#define ALIGN16(x)   (((x) + 15) & ~(uint64_t)15)
#define MALLOC_MAGIC 0x54474C41u          /* 'TLGA' */
#define HEADER_SIZE  16

/* Block layout (16-byte header):
 *   size  : usable payload size          (offset 0)
 *   meta  : next free block, or magic    (offset 8, reused on free)
 */
struct block {
    uint64_t size;
    uint64_t meta;
};

static struct block *free_list = NULL;
static uintptr_t bump = 0;
static uintptr_t bump_end = 0;
static uint64_t total_bytes = 0;
static uint64_t used_bytes = 0;
static uint32_t block_count = 0;

static void grow_heap(uint64_t need)
{
    uint64_t chunk = need > 65536 ? need : 65536;   /* at least 64 KiB */
    size_t pages = (size_t)((chunk + 4095) / 4096);
    uintptr_t base = tpmm_alloc_contig(pages);
    if (!base)
        return;
    bump = base + HEADER_SIZE;
    bump_end = base + (uintptr_t)pages * 4096;
    total_bytes += (uint64_t)pages * 4096;
}

void *tmalloc(size_t size)
{
    uint64_t want = ALIGN16(size);
    if (want < 16)
        want = 16;

    /* first-fit over the free list, splitting oversized blocks */
    struct block **pp = &free_list;
    while (*pp) {
        struct block *b = *pp;
        if (b->size >= want) {
            *pp = (struct block *)(uintptr_t)b->meta;
            if (b->size >= want + HEADER_SIZE + 16) {
                struct block *rest =
                    (struct block *)((uintptr_t)b + HEADER_SIZE + want);
                rest->size = b->size - want - HEADER_SIZE;
                rest->meta = (uint64_t)(uintptr_t)*pp;
                *pp = rest;
            }
            b->size = want;
            b->meta = MALLOC_MAGIC;
            used_bytes += b->size;
            block_count++;
            return (void *)b;
        }
        pp = (struct block **)&b->meta;
    }

    /* bump carve, growing the heap if needed */
    if (bump + want + HEADER_SIZE > bump_end)
        grow_heap(want + HEADER_SIZE);
    if (bump + want + HEADER_SIZE > bump_end)
        return NULL;

    struct block *b = (struct block *)bump;
    b->size = want;
    b->meta = MALLOC_MAGIC;
    bump += HEADER_SIZE + want;
    used_bytes += want;
    block_count++;
    return (void *)b;
}

void tfree(void *ptr)
{
    if (!ptr)
        return;
    struct block *b = (struct block *)ptr;
    if (b->meta != MALLOC_MAGIC)
        return;                       /* double free / bogus pointer */
    used_bytes -= b->size;
    block_count--;
    b->meta = (uint64_t)(uintptr_t)free_list;
    free_list = b;
}

uint64_t tmalloc_total(void)
{
    return total_bytes;
}

uint64_t tmalloc_used(void)
{
    return used_bytes;
}

uint32_t tmalloc_blocks(void)
{
    return block_count;
}
