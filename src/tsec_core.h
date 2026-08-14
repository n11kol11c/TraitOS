#ifndef TSEC_CORE_H
#define TSEC_CORE_H

#include <stdint.h>
#include <stddef.h>

#include "tvmm.h"

#define TSEC_PAGE_NX (1ull << 63)

/* The higher-half physmap maps physical p to virtual VIRT_BASE + p, so the
 * kernel image (loaded at phys PHYS_BASE) occupies the window offsets
 * [IMAGE_LOCAL, IMAGE_LOCAL + image_size). Physical KASLR overrides exactly
 * those PTEs to point at a relocated copy (reloc_base != 0); the original
 * copy stays reserved. Everything else keeps the physmap identity.
 *
 * These helpers are pure (no kernel state) so `make smoke` can verify the
 * exact arithmetic and flags the boot-time hardening uses. */

/* Window-local bounds of the executable (.text), read-only (.rodata) and
 * writable (.data/.bss) regions, plus the end of the whole image. */
static inline void tsec_region_bounds(uintptr_t vbase, uintptr_t phys_base,
                                      uintptr_t image_local,
                                      uintptr_t kvstart, uintptr_t etext,
                                      uintptr_t erodata, uintptr_t kvend,
                                      uintptr_t *text_lo, uintptr_t *text_hi,
                                      uintptr_t *rodata_hi,
                                      uintptr_t *image_end)
{
    *text_lo = image_local + (kvstart - vbase) - phys_base;
    *text_hi = image_local + (etext - vbase) - phys_base;
    *rodata_hi = image_local + (erodata - vbase) - phys_base;
    *image_end = image_local + (kvend - vbase) - phys_base;
}

/* PTE for a window-local offset: code read-only+executable, rodata
 * read-only, everything else writable+non-executable (W^X). */
static inline uint64_t tsec_pte_for_off(uintptr_t off, uintptr_t reloc_base,
                                        uintptr_t image_local,
                                        uintptr_t image_end,
                                        uintptr_t text_lo, uintptr_t text_hi,
                                        uintptr_t rodata_hi)
{
    uint64_t flags;
    if (off >= text_lo && off < text_hi)
        flags = 0;                       /* X, read-only */
    else if (off >= text_hi && off < rodata_hi)
        flags = TSEC_PAGE_NX;            /* read-only */
    else
        flags = VMM_PAGE_WRITE | TSEC_PAGE_NX;

    uintptr_t phys = off;
    if (reloc_base && off >= image_local && off < image_end)
        phys = reloc_base + (off - image_local);
    return (uint64_t)phys | VMM_PAGE_PRESENT | flags;
}

/* Fill a 4 KiB page table covering one 2 MiB higher-half window. */
static inline void tsec_fill_window(uint64_t *pt, uintptr_t reloc_base,
                                    uintptr_t image_local, uintptr_t text_lo,
                                    uintptr_t text_hi, uintptr_t rodata_hi,
                                    uintptr_t image_end)
{
    for (unsigned i = 0; i < 512; i++) {
        uintptr_t off = (uintptr_t)i * 4096;
        pt[i] = tsec_pte_for_off(off, reloc_base, image_local, image_end,
                                 text_lo, text_hi, rodata_hi);
    }
}

/* Identity map for the low 2 MiB (the boot leaf below the kernel), 4 KiB
 * pages, 1:1 phys, writable+non-executable — with the page at `guard`
 * (4 KiB aligned) left non-present. The kernel stack lives at 1 MiB just
 * above the guard, so a stack overflow faults instead of silently
 * overwriting the boot page tables below it. */
static inline void tsec_fill_guard_pt(uint64_t *pt, uintptr_t guard)
{
    for (unsigned i = 0; i < 512; i++) {
        uintptr_t phys = (uintptr_t)i * 4096;
        if (guard && phys >= guard && phys < guard + 4096)
            pt[i] = 0;                   /* non-present: guard page */
        else
            pt[i] = phys | VMM_PAGE_PRESENT | VMM_PAGE_WRITE |
                    TSEC_PAGE_NX;
    }
}

#endif
