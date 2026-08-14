#ifndef TELF_CORE_H
#define TELF_CORE_H

/* Pure ELF64 executable parser (M6c), host-testable like the ttask/tipc
 * cores: every function is a static inline that only reads the input blob,
 * so `make smoke` can link it directly and verify the exact parse math the
 * kernel loader uses. telf.c wraps it with address-space mapping. */

#include <stdint.h>
#include <stddef.h>

#define TELF_MAX_SEGMENTS 4

#define TELF_PROT_READ  1
#define TELF_PROT_WRITE 2
#define TELF_PROT_EXEC  4

typedef struct {
    uintptr_t vaddr;      /* segment vaddr (page-aligned by the linker) */
    uint32_t  offset;     /* byte offset into the ELF file */
    uint32_t  filesz;     /* bytes to load from the file */
    uint32_t  memsz;      /* bytes to reserve in memory (>= filesz) */
    uint8_t   prot;       /* TELF_PROT_* bits */
} telf_segment_t;

typedef struct {
    uintptr_t      entry;     /* program entry point (vaddr) */
    int            nsegs;
    telf_segment_t segs[TELF_MAX_SEGMENTS];
} telf_plan_t;

#define TELF_OK             0
#define TELF_E_BAD_MAGIC   -1
#define TELF_E_BAD_CLASS   -2
#define TELF_E_BAD_DATA    -3
#define TELF_E_BAD_TYPE    -4
#define TELF_E_BAD_MACHINE -5
#define TELF_E_BAD_VERSION -6
#define TELF_E_TRUNC       -7
#define TELF_E_NO_LOAD     -8
#define TELF_E_TOO_MANY    -9
#define TELF_E_RANGE       -10
#define TELF_E_BAD_ALIGN   -11
#define TELF_E_PHDR        -12

/* ELF64 header + program header field offsets (little-endian). */
#define TELF_EHDR_TYPE   16
#define TELF_EHDR_MACH   18
#define TELF_EHDR_VER    20
#define TELF_EHDR_ENTRY  24
#define TELF_EHDR_PHOFF  32
#define TELF_EHDR_PHENTSZ 54
#define TELF_EHDR_PHNUM   56

#define TELF_PHDR_TYPE   0
#define TELF_PHDR_FLAGS  4
#define TELF_PHDR_OFF    8
#define TELF_PHDR_VADDR  16
#define TELF_PHDR_FILESZ 32
#define TELF_PHDR_MEMSZ  40
#define TELF_PHDR_ALIGN  48
#define TELF_PHDR_SZ     56

#define TELF_ET_EXEC   2
#define TELF_EM_X86_64 62
#define TELF_PT_LOAD   1

static inline uint16_t telf_rd16(const uint8_t *b, size_t off)
{
    return (uint16_t)((uint16_t)b[off] | ((uint16_t)b[off + 1] << 8));
}

static inline uint32_t telf_rd32(const uint8_t *b, size_t off)
{
    return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) |
           ((uint32_t)b[off + 2] << 16) | ((uint32_t)b[off + 3] << 24);
}

static inline uint64_t telf_rd64(const uint8_t *b, size_t off)
{
    return (uint64_t)telf_rd32(b, off) | ((uint64_t)telf_rd32(b, off + 4) << 32);
}

static inline int telf_parse(const uint8_t *blob, size_t size,
                             telf_plan_t *plan)
{
    if (size < 64)
        return TELF_E_TRUNC;
    if (blob[0] != 0x7F || blob[1] != 'E' || blob[2] != 'L' ||
        blob[3] != 'F')
        return TELF_E_BAD_MAGIC;
    if (blob[4] != 2)
        return TELF_E_BAD_CLASS;             /* 64-bit only */
    if (blob[5] != 1)
        return TELF_E_BAD_DATA;              /* little-endian only */
    if (blob[6] != 1)
        return TELF_E_BAD_VERSION;
    if (telf_rd16(blob, TELF_EHDR_TYPE) != TELF_ET_EXEC)
        return TELF_E_BAD_TYPE;
    if (telf_rd16(blob, TELF_EHDR_MACH) != TELF_EM_X86_64)
        return TELF_E_BAD_MACHINE;
    if (telf_rd32(blob, TELF_EHDR_VER) != 1)
        return TELF_E_BAD_VERSION;

    uint64_t entry = telf_rd64(blob, TELF_EHDR_ENTRY);
    uint64_t phoff = telf_rd64(blob, TELF_EHDR_PHOFF);
    uint16_t phentsz = telf_rd16(blob, TELF_EHDR_PHENTSZ);
    uint16_t phnum = telf_rd16(blob, TELF_EHDR_PHNUM);

    if (phentsz < TELF_PHDR_SZ)
        return TELF_E_PHDR;
    if (phnum == 0)
        return TELF_E_NO_LOAD;
    if (phnum > TELF_MAX_SEGMENTS)
        return TELF_E_TOO_MANY;

    int n = 0;
    for (int i = 0; i < phnum; i++) {
        uint64_t off = phoff + (uint64_t)i * phentsz;
        if (off + TELF_PHDR_SZ > size)
            return TELF_E_TRUNC;
        if (telf_rd32(blob, off + TELF_PHDR_TYPE) != TELF_PT_LOAD)
            continue;
        if (n == TELF_MAX_SEGMENTS)
            return TELF_E_TOO_MANY;

        uint64_t flags  = telf_rd32(blob, off + TELF_PHDR_FLAGS);
        uint64_t poff   = telf_rd64(blob, off + TELF_PHDR_OFF);
        uint64_t vaddr  = telf_rd64(blob, off + TELF_PHDR_VADDR);
        uint64_t filesz = telf_rd64(blob, off + TELF_PHDR_FILESZ);
        uint64_t memsz  = telf_rd64(blob, off + TELF_PHDR_MEMSZ);
        uint64_t align  = telf_rd64(blob, off + TELF_PHDR_ALIGN);

        if (poff > size || filesz > size - poff)
            return TELF_E_TRUNC;             /* file bytes out of bounds */
        if (memsz < filesz)
            return TELF_E_RANGE;
        if (vaddr > UINT64_MAX - memsz)
            return TELF_E_RANGE;             /* vaddr + memsz would wrap */
        if (align != 0 && (align & (align - 1)) != 0)
            return TELF_E_BAD_ALIGN;         /* not a power of two */
        if (align > 1 && (vaddr % align) != (poff % align))
            return TELF_E_BAD_ALIGN;         /* ELF alignment requirement */

        telf_segment_t *s = &plan->segs[n];
        s->vaddr = (uintptr_t)vaddr;
        s->offset = (uint32_t)poff;
        s->filesz = (uint32_t)filesz;
        s->memsz = (uint32_t)memsz;
        s->prot = (uint8_t)((flags & 4 ? TELF_PROT_READ : 0) |
                            (flags & 2 ? TELF_PROT_WRITE : 0) |
                            (flags & 1 ? TELF_PROT_EXEC : 0));
        n++;
    }

    if (n == 0)
        return TELF_E_NO_LOAD;

    /* reject overlapping page ranges between segments */
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            uintptr_t ai = plan->segs[i].vaddr & ~(uintptr_t)0xFFF;
            uintptr_t bi = plan->segs[i].vaddr + plan->segs[i].memsz;
            uintptr_t aj = plan->segs[j].vaddr & ~(uintptr_t)0xFFF;
            uintptr_t bj = plan->segs[j].vaddr + plan->segs[j].memsz;
            if (ai < bj && aj < bi)
                return TELF_E_RANGE;
        }
    }

    plan->entry = (uintptr_t)entry;
    plan->nsegs = n;
    return TELF_OK;
}

#endif
