#include "telf.h"

#include "telf_core.h"
#include "tpmm.h"
#include "tstring.h"
#include "tvmm.h"

int telf_load(vmm_aspace_t *as, const uint8_t *blob, size_t size,
              telf_plan_t *plan)
{
    int rc = telf_parse(blob, size, plan);
    if (rc != TELF_OK)
        return rc;

    for (int i = 0; i < plan->nsegs; i++) {
        telf_segment_t *s = &plan->segs[i];

        if (s->vaddr < VMM_USER_BASE || s->vaddr + s->memsz > VMM_USER_END)
            return TELF_E_RANGE;

        uintptr_t first = s->vaddr & ~(uintptr_t)0xFFF;
        uintptr_t last = (s->vaddr + s->memsz - 1) & ~(uintptr_t)0xFFF;
        size_t pages = (last - first) / 4096 + 1;

        uintptr_t phys = tpmm_alloc_low_contig(pages);
        if (!phys)
            return TELF_E_RANGE;

        uint64_t flags = VMM_PAGE_USER;
        if (s->prot & TELF_PROT_WRITE)
            flags |= VMM_PAGE_WRITE;
        if (!(s->prot & TELF_PROT_EXEC))
            flags |= VMM_PAGE_NX;

        for (size_t p = 0; p < pages; p++)
            if (vmm_aspace_map(as, first + p * 4096, phys + p * 4096, flags))
                return TELF_E_RANGE;

        /* frames live below 1 GiB, so the physmap alias can fill them in
         * without touching the user CR3 */
        uint8_t *dst = (uint8_t *)VMM_PHYS_TO_VIRT(phys);
        tmemcpy(dst, blob + s->offset, s->filesz);
        if (s->memsz > s->filesz)
            tmemset(dst + s->filesz, 0, s->memsz - s->filesz);
    }
    return TELF_OK;
}
