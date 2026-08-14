/* Host-side verification of the M6c ELF64 parser (src/telf_core.h). The
 * kernel loader uses the exact same pure function; here we build minimal
 * ELF blobs by hand and check both acceptance and every rejection path. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "telf_core.h"

static int checks = 0;
static int failures = 0;

static void check(int cond, const char *what)
{
    checks++;
    if (!cond) {
        failures++;
        printf("  FAIL %s\n", what);
    }
}

enum { BUF_SZ = 16384 };

static void wr16(uint8_t *b, size_t o, uint16_t v)
{
    b[o] = (uint8_t)(v & 0xFF);
    b[o + 1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t *b, size_t o, uint32_t v)
{
    b[o] = (uint8_t)(v & 0xFF);
    b[o + 1] = (uint8_t)((v >> 8) & 0xFF);
    b[o + 2] = (uint8_t)((v >> 16) & 0xFF);
    b[o + 3] = (uint8_t)((v >> 24) & 0xFF);
}

static void wr64(uint8_t *b, size_t o, uint64_t v)
{
    wr32(b, o, (uint32_t)(v & 0xFFFFFFFF));
    wr32(b, o + 4, (uint32_t)(v >> 32));
}

/* Build a minimal ELF64 ET_EXEC with the given number of PT_LOAD phdrs.
 * The first phdr table starts right after the 64-byte header. */
static size_t build_elf(uint8_t *b, uint16_t phnum)
{
    memset(b, 0, BUF_SZ);
    b[0] = 0x7F;
    b[1] = 'E';
    b[2] = 'L';
    b[3] = 'F';
    b[4] = 2;                 /* ELFCLASS64 */
    b[5] = 1;                 /* little-endian */
    b[6] = 1;                 /* EV_CURRENT */
    wr16(b, TELF_EHDR_TYPE, TELF_ET_EXEC);
    wr16(b, TELF_EHDR_MACH, TELF_EM_X86_64);
    wr32(b, TELF_EHDR_VER, 1);
    wr64(b, TELF_EHDR_ENTRY, 0x8000000000ULL);
    wr64(b, TELF_EHDR_PHOFF, 64);
    wr16(b, TELF_EHDR_PHENTSZ, TELF_PHDR_SZ);
    wr16(b, TELF_EHDR_PHNUM, phnum);

    for (uint16_t i = 0; i < phnum && i < TELF_MAX_SEGMENTS + 1; i++) {
        size_t o = 64 + (size_t)i * TELF_PHDR_SZ;
        wr32(b, o + TELF_PHDR_TYPE, TELF_PT_LOAD);
        wr32(b, o + TELF_PHDR_FLAGS, 5);           /* R + X */
        wr64(b, o + TELF_PHDR_OFF, 0x1000 + (uint64_t)i * 0x1000);
        wr64(b, o + TELF_PHDR_VADDR, 0x8000000000ULL + (uint64_t)i * 0x1000);
        wr64(b, o + TELF_PHDR_FILESZ, 0x100);
        wr64(b, o + TELF_PHDR_MEMSZ, 0x100);
        wr64(b, o + TELF_PHDR_ALIGN, 0x1000);
    }
    return 64 + (size_t)phnum * TELF_PHDR_SZ;
}

int main(void)
{
    uint8_t buf[BUF_SZ];
    telf_plan_t plan;

    printf("telf ELF parser smoke\n");

    printf("  -- valid single-segment --\n");
    size_t sz = build_elf(buf, 1);
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_OK, "single segment parses");
    check(plan.entry == 0x8000000000ULL, "entry recorded");
    check(plan.nsegs == 1, "one segment");
    check(plan.segs[0].vaddr == 0x8000000000ULL, "segment vaddr");
    check(plan.segs[0].filesz == 0x100 && plan.segs[0].memsz == 0x100,
          "segment sizes");
    check(plan.segs[0].prot == (TELF_PROT_READ | TELF_PROT_EXEC),
          "segment prot R+X");

    printf("  -- valid multi-segment (writable bss) --\n");
    sz = build_elf(buf, 2);
    wr32(buf, 64 + TELF_PHDR_FLAGS, 2);            /* segment 0: W only */
    wr64(buf, 64 + TELF_PHDR_MEMSZ, 0x200);        /* bss tail */
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_OK, "two segments parse");
    check(plan.nsegs == 2, "two segments counted");
    check(plan.segs[0].prot == TELF_PROT_WRITE, "writable prot mapped");
    check(plan.segs[0].memsz > plan.segs[0].filesz, "bss grows memsz");

    printf("  -- header rejections --\n");
    check(telf_parse(buf, 63, &plan) == TELF_E_TRUNC, "short blob");
    buf[1] = 'x';
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_BAD_MAGIC, "bad magic");
    buf[1] = 'E';
    buf[4] = 1;
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_BAD_CLASS, "32-bit rejected");
    buf[4] = 2;
    buf[5] = 2;
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_BAD_DATA, "big-endian rejected");
    buf[5] = 1;
    wr16(buf, TELF_EHDR_TYPE, 3);                  /* ET_DYN */
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_BAD_TYPE, "non-exec rejected");
    wr16(buf, TELF_EHDR_TYPE, TELF_ET_EXEC);
    wr16(buf, TELF_EHDR_MACH, 3);                  /* EM_386 */
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_BAD_MACHINE, "wrong machine");
    wr16(buf, TELF_EHDR_MACH, TELF_EM_X86_64);
    wr32(buf, TELF_EHDR_VER, 2);
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_BAD_VERSION, "bad version");
    wr32(buf, TELF_EHDR_VER, 1);

    printf("  -- program header rejections --\n");
    sz = build_elf(buf, 0);
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_NO_LOAD, "no phdrs");
    sz = build_elf(buf, TELF_MAX_SEGMENTS + 1);
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_TOO_MANY, "too many load segs");
    sz = build_elf(buf, 1);
    wr64(buf, TELF_EHDR_PHOFF, BUF_SZ - TELF_PHDR_SZ + 1);
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_TRUNC, "phdr table truncated");
    wr64(buf, TELF_EHDR_PHOFF, 64);
    wr64(buf, 64 + TELF_PHDR_OFF, BUF_SZ);             /* past the blob end */
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_TRUNC, "file bytes out of bounds");
    wr64(buf, 64 + TELF_PHDR_OFF, 0x1000);
    wr64(buf, 64 + TELF_PHDR_MEMSZ, 0x50);             /* memsz < filesz */
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_RANGE, "memsz < filesz");
    wr64(buf, 64 + TELF_PHDR_MEMSZ, 0x100);
    wr64(buf, 64 + TELF_PHDR_VADDR, 0xFFFFFFFFFFFFF000ULL); /* wraps with 0x1000 */
    wr64(buf, 64 + TELF_PHDR_MEMSZ, 0x1000);
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_RANGE, "vaddr wraps");
    wr64(buf, 64 + TELF_PHDR_VADDR, 0x8000000000ULL);
    wr64(buf, 64 + TELF_PHDR_ALIGN, 3);                /* not a power of two */
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_BAD_ALIGN, "bad align value");
    wr64(buf, 64 + TELF_PHDR_ALIGN, 0x1000);
    wr64(buf, 64 + TELF_PHDR_OFF, 0x1001);             /* vaddr%align != off%align */
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_BAD_ALIGN, "misaligned offset");
    wr64(buf, 64 + TELF_PHDR_OFF, 0x1000);

    printf("  -- overlapping segments --\n");
    sz = build_elf(buf, 2);
    wr64(buf, 64 + TELF_PHDR_SZ + TELF_PHDR_VADDR, 0x8000000000ULL); /* seg1 over seg0 */
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_E_RANGE, "overlap rejected");

    printf("  -- non-load phdr skipped --\n");
    sz = build_elf(buf, 2);
    wr32(buf, 64 + TELF_PHDR_TYPE, 7);                 /* PT_TLS ignored */
    check(telf_parse(buf, BUF_SZ, &plan) == TELF_OK, "non-load skipped");
    check(plan.nsegs == 1, "only load counted");

    if (failures) {
        printf("TELF SMOKE TEST FAILED (%d/%d failures)\n", failures, checks);
        return 1;
    }
    printf("TELF SMOKE TEST PASSED (%d checks)\n", checks);
    return 0;
}
