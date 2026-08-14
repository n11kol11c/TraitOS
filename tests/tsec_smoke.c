/* Host-side verification of the M5 security core (W^X split + physical KASLR
 * PTE math) in src/tsec_core.h. Uses synthetic but realistic section VMA
 * values; the kernel calls the exact same pure helpers at boot. */
#include <stdio.h>
#include <stdint.h>

#include "tsec_core.h"
#include "tvmm.h"

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

static void verify_window(uintptr_t reloc_base, uintptr_t image_local,
                          uintptr_t text_lo, uintptr_t text_hi,
                          uintptr_t rodata_hi, uintptr_t image_end)
{
    uint64_t pt[512];
    tsec_fill_window(pt, reloc_base, image_local, text_lo, text_hi,
                     rodata_hi, image_end);

    for (unsigned i = 0; i < 512; i++) {
        uintptr_t off = (uintptr_t)i * 4096;
        uint64_t pte = pt[i];
        char what[128];
        int w = (pte & VMM_PAGE_WRITE) != 0;
        int x = (pte & TSEC_PAGE_NX) == 0;
        int present = (pte & VMM_PAGE_PRESENT) != 0;
        uintptr_t phys = (uintptr_t)(pte & 0x000FFFFFFFFFF000ULL);

        if (!present)
            continue;

        /* W^X: no page is both writable and executable */
        snprintf(what, sizeof what, "offset 0x%x: W^X", (unsigned)off);
        check(!(w && x), what);

        /* code pages: executable, read-only, never relocated wrongly */
        if (off >= text_lo && off < text_hi) {
            snprintf(what, sizeof what, "offset 0x%x: text X", (unsigned)off);
            check(x && !w, what);
            snprintf(what, sizeof what, "offset 0x%x: text phys",
                     (unsigned)off);
            check(phys == (reloc_base ? reloc_base + (off - image_local)
                                      : off),
                  what);
        }

        /* rodata: read-only, non-executable */
        if (off >= text_hi && off < rodata_hi) {
            snprintf(what, sizeof what, "offset 0x%x: rodata RO+NX",
                     (unsigned)off);
            check(!w && !x, what);
        }

        /* data and everything outside the image: writable, NX */
        if (off >= rodata_hi || off < text_lo || off >= image_end) {
            snprintf(what, sizeof what, "offset 0x%x: data RW+NX",
                     (unsigned)off);
            check(w && !x, what);
        }

        /* non-image pages keep the physmap identity */
        if (off < image_local || off >= image_end) {
            snprintf(what, sizeof what, "offset 0x%x: identity phys",
                     (unsigned)off);
            check(phys == off, what);
        }
    }
}

int main(void)
{
    uintptr_t vbase = 0xFFFFFFFF80000000ULL;
    uintptr_t phys_base = 0x100000;
    uintptr_t image_local = 0x100000;

    uintptr_t kvstart = vbase + 0x111000;
    uintptr_t etext = vbase + 0x114000;
    uintptr_t erodata = vbase + 0x121000;
    uintptr_t kvend = vbase + 0x160000;   /* image size 0x60000 */

    uintptr_t text_lo, text_hi, rodata_hi, image_end;
    tsec_region_bounds(vbase, phys_base, image_local, kvstart, etext,
                       erodata, kvend, &text_lo, &text_hi, &rodata_hi,
                       &image_end);

    printf("tsec core smoke\n");
    printf("  text  [0x%lx, 0x%lx)\n", (unsigned long)text_lo,
           (unsigned long)text_hi);
    printf("  rodata[0x%lx, 0x%lx)\n", (unsigned long)text_hi,
           (unsigned long)rodata_hi);
    printf("  image [0x%lx, 0x%lx)\n", (unsigned long)image_local,
           (unsigned long)image_end);

    check(text_lo == 0x111000 && text_hi == 0x114000,
          "text bounds (no KASLR)");
    check(rodata_hi == 0x121000 && image_end == 0x160000,
          "rodata + image bounds (no KASLR)");

    printf("  -- W^X split, kernel at 1 MiB --\n");
    verify_window(0, image_local, text_lo, text_hi, rodata_hi, image_end);

    printf("  -- W^X split, kernel relocated to 64 MiB --\n");
    verify_window(0x04000000, image_local, text_lo, text_hi, rodata_hi,
                  image_end);

    if (failures) {
        printf("TSEC SMOKE TEST FAILED (%d/%d failures)\n", failures,
               checks);
        return 1;
    }
    printf("TSEC SMOKE TEST PASSED (%d checks)\n", checks);
    return 0;
}
