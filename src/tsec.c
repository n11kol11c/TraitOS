#include "tsec.h"

#include "tsec_core.h"
#include "tvmm.h"
#include "tpmm.h"
#include "tstring.h"

#include <stddef.h>

/* Linker-provided section boundaries (see linker.ld). */
extern char _kernel_phys_start[];
extern char _kernel_phys_end[];
extern char _kernel_virtual_start[];
extern char _kernel_virtual_end[];
extern char _etext[], _erodata[];

#define EFER_MSR 0xC0000080ULL
#define EFER_NXE (1ull << 11)

#define CR4_SMEP (1ull << 20)
#define CR4_SMAP (1ull << 21)

#define NX TSEC_PAGE_NX
#define PS (1ull << 7)
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define PAGE_SIZE  4096
#define PML4_SLOT(p)  (((uintptr_t)(p) >> 39) & 0x1FF)
#define PDPT_SLOT(p)  (((uintptr_t)(p) >> 30) & 0x1FF)
#define PD_SLOT(p)    (((uintptr_t)(p) >> 21) & 0x1FF)
#define PT_SLOT(p)    (((uintptr_t)(p) >> 12) & 0x1FF)

/* KASLR slot window: 2 MiB-aligned slots in [32 MiB, 128 MiB). */
#define KASLR_WIN_LO  0x02000000u
#define KASLR_WIN_HI  0x08000000u
#define WINDOW_SIZE   (2u * 1024u * 1024u)

/* The image loads at physical 1 MiB, so its VMA range (VIRT_BASE + p) starts
 * at window offset IMAGE_LOCAL. Physical KASLR overrides exactly those PTEs
 * to point at the relocated copy; the original copy stays reserved. */
#define IMAGE_LOCAL   0x100000u

static uintptr_t kaslr_base = 0;

static uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static void wrmsr(uint32_t msr, uint64_t v)
{
    __asm__ volatile("wrmsr"
                     : : "a"((uint32_t)v), "d"((uint32_t)(v >> 32)),
                         "c"(msr));
}

static uint64_t *table_of(uint64_t e)
{
    return (uint64_t *)(uintptr_t)VMM_PHYS_TO_VIRT(e & PTE_ADDR_MASK);
}

static uint32_t boot_entropy(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    uint32_t e = lo ^ (hi << 9) ^ (uint32_t)(uintptr_t)&kaslr_base;
    e ^= e << 13;
    e ^= e >> 17;
    e ^= e << 5;
    return e;
}

void sec_enable_nxe(void)
{
    uint64_t efer = rdmsr(EFER_MSR);
    if (!(efer & EFER_NXE))
        wrmsr(EFER_MSR, efer | EFER_NXE);
}

void sec_enable_smep_smap(void)
{
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4 | CR4_SMEP | CR4_SMAP)
                     : "memory");
}

/* Multiboot2 module tag (type 3): the initrd. KASLR must not overwrite it. */
static int module_range(uintptr_t mbi, uintptr_t *start, uintptr_t *end)
{
    uintptr_t p = mbi + 8;
    uintptr_t limit = mbi + *(uint32_t *)mbi;
    while (p + 8 <= limit) {
        uint32_t type = *(uint32_t *)p;
        uint32_t size = *(uint32_t *)(p + 4);
        if (type == 0)
            break;
        if (type == 3 && size >= 24) {
            *start = *(uint32_t *)(p + 8);
            *end = *(uint32_t *)(p + 12);
            return 1;
        }
        p += (size + 7) & ~(uintptr_t)7;
    }
    return 0;
}

/* The physical KASLR override re-points the higher-half VMA of the image
 * range to the relocated copy. The live boot page tables live *inside* that
 * range (in .boot.bss at 1 MiB), so they would lose their window alias.
 * Copy the five-table boot tree to fresh frames and switch CR3 first, so
 * every live table frame sits outside the overridden range. */
static int relocate_page_tables(void)
{
    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *p4 = table_of(cr3);

    uintptr_t n4p = tpmm_alloc_low();
    uintptr_t n3p = tpmm_alloc_low();
    uintptr_t n2p = tpmm_alloc_low();
    uintptr_t n3hp = tpmm_alloc_low();
    uintptr_t n2hp = tpmm_alloc_low();
    if (!n4p || !n3p || !n2p || !n3hp || !n2hp) {
        if (n4p) tpmm_free(n4p);
        if (n3p) tpmm_free(n3p);
        if (n2p) tpmm_free(n2p);
        if (n3hp) tpmm_free(n3hp);
        if (n2hp) tpmm_free(n2hp);
        return -1;
    }

    uint64_t *n4 = table_of(n4p);
    uint64_t *n3 = table_of(n3p);
    uint64_t *n2 = table_of(n2p);
    uint64_t *n3h = table_of(n3hp);
    uint64_t *n2h = table_of(n2hp);

    tmemmove(n4, p4, 4096);
    tmemmove(n3, table_of(p4[0]), 4096);
    tmemmove(n2, table_of(p4[0] ? table_of(p4[0])[0] : 0), 4096);
    tmemmove(n3h, table_of(p4[511]), 4096);
    tmemmove(n2h, table_of(p4[511] ? table_of(p4[511])[510] : 0), 4096);

    n4[0] = n3p | (n4[0] & ~PTE_ADDR_MASK);
    n4[511] = n3hp | (n4[511] & ~PTE_ADDR_MASK);
    n3[0] = n2p | (n3[0] & ~PTE_ADDR_MASK);
    n3h[510] = n2hp | (n3h[510] & ~PTE_ADDR_MASK);

    __asm__ volatile("mov %0, %%cr3" : : "r"(n4p) : "memory");
    __asm__ volatile("mov %%cr3, %0" : : "r"(n4p) : "memory");   /* flush */
    return 0;
}

int sec_kaslr_relocate(uintptr_t mbi)
{
    uintptr_t image = (uintptr_t)_kernel_phys_start;
    size_t image_size =
        (size_t)((uintptr_t)_kernel_phys_end - (uintptr_t)_kernel_phys_start);

    uintptr_t mod_start = 0, mod_end = 0;
    int have_mod = module_range(mbi, &mod_start, &mod_end);

    unsigned slots = (KASLR_WIN_HI - KASLR_WIN_LO) / WINDOW_SIZE;
    unsigned start_slot = boot_entropy() % slots;

    for (unsigned n = 0; n < slots; n++) {
        unsigned slot = (start_slot + n) % slots;
        uintptr_t base = KASLR_WIN_LO + (uintptr_t)slot * WINDOW_SIZE;

        if (have_mod && base < mod_end && base + WINDOW_SIZE > mod_start)
            continue;
        if (base < image + image_size && base + WINDOW_SIZE > image)
            continue;

        int free_ok = 1;
        for (size_t i = 0; i < WINDOW_SIZE / PAGE_SIZE; i++)
            if (tpmm_frame_used(base + i * PAGE_SIZE)) {
                free_ok = 0;
                break;
            }
        if (!free_ok)
            continue;

        /* the original copy stays reserved (dead), the relocated copy is
         * claimed too; frames never leave these ranges for the allocator */
        tpmm_reserve_range(base, image_size);

        /* move the live page tables out of the to-be-overridden VMA range */
        if (relocate_page_tables() != 0) {
            tpmm_release_range(base, image_size);
            return 0;
        }

        tmemmove((void *)(uintptr_t)VMM_PHYS_TO_VIRT(base),
                 (void *)(uintptr_t)VMM_PHYS_TO_VIRT(image), image_size);
        kaslr_base = base;
        return 1;
    }
    return 0;
}

uintptr_t sec_kernel_phys_base(void)
{
    return kaslr_base;
}

/* Split higher-half PD entry 0 (VMA [VIRT_BASE, VIRT_BASE + 2 MiB)) into
 * 4 KiB pages: code read-only+executable, everything else non-executable
 * (W^X). When kaslr_base != 0, the image's PTEs point at the relocated
 * copy; every other PTE keeps the physmap identity (phys = window offset). */
static int sec_split_window(uint64_t *pd, uintptr_t reloc_base)
{
    uintptr_t pt_frame = tpmm_alloc_low();
    if (!pt_frame)
        return -1;

    uint64_t *pt = table_of(pt_frame);
    tmemset(pt, 0, 4096);

    uintptr_t text_lo, text_hi, rodata_hi, image_end;
    tsec_region_bounds((uintptr_t)VMM_VIRT_BASE,
                       (uintptr_t)_kernel_phys_start, IMAGE_LOCAL,
                       (uintptr_t)_kernel_virtual_start, (uintptr_t)_etext,
                       (uintptr_t)_erodata, (uintptr_t)_kernel_virtual_end,
                       &text_lo, &text_hi, &rodata_hi, &image_end);

    tsec_fill_window(pt, reloc_base, IMAGE_LOCAL, text_lo, text_hi,
                     rodata_hi, image_end);

    pd[0] = pt_frame | VMM_PAGE_PRESENT | VMM_PAGE_WRITE;
    return 0;
}

static void sec_flush_tlb(void)
{
    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

void sec_harden(void)
{
    sec_enable_nxe();
    sec_enable_smep_smap();

    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = table_of(cr3);

    /* identity map: nothing below 1 GiB should ever execute */
    uint64_t *p3_lo = table_of(pml4[0]);
    uint64_t *p2_lo = table_of(p3_lo[0]);
    for (int i = 0; i < 512; i++)
        p2_lo[i] |= NX;

    /* higher-half physmap: NX every 2 MiB leaf except entry 0, which is
     * replaced by the W^X 4 KiB split below */
    uint64_t *p3_hi = table_of(pml4[511]);
    uint64_t *pd = table_of(p3_hi[510]);
    for (int i = 1; i < 512; i++)
        pd[i] |= NX;

    sec_split_window(pd, kaslr_base);
    sec_flush_tlb();
}

/* Leaf flags for a virtual address (0 if unmapped). */
static uint64_t sec_leaf_flags(uintptr_t vaddr)
{
    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = table_of(cr3);
    uint64_t *p3 = table_of(pml4[PML4_SLOT(vaddr)]);
    uint64_t e3 = p3[PDPT_SLOT(vaddr)];
    if (!(e3 & VMM_PAGE_PRESENT))
        return 0;
    if (e3 & PS)
        return e3;
    uint64_t *pd = table_of(e3);
    uint64_t e2 = pd[PD_SLOT(vaddr)];
    if (!(e2 & VMM_PAGE_PRESENT))
        return 0;
    if (e2 & PS)
        return e2;
    uint64_t *pt = table_of(e2);
    return pt[PT_SLOT(vaddr)];
}

int sec_wx_enforced(void)
{
    for (size_t i = 0; i < WINDOW_SIZE / PAGE_SIZE; i++) {
        uint64_t flags = sec_leaf_flags((uintptr_t)VMM_VIRT_BASE +
                                        i * PAGE_SIZE);
        int w = (flags & VMM_PAGE_WRITE) != 0;
        int x = (flags & NX) == 0;
        if (w && x)
            return 0;
    }
    return 1;
}

int sec_nx_enforced(void)
{
    /* every higher-half physmap leaf except the W^X window must be NX */
    for (unsigned i = 1; i < 512; i++) {
        uint64_t flags = sec_leaf_flags((uintptr_t)VMM_VIRT_BASE +
                                        (uintptr_t)i * 2 * 1024 * 1024);
        if (flags && !(flags & NX))
            return 0;
    }
    return 1;
}

uint64_t sec_read_efer(void)
{
    return rdmsr(EFER_MSR);
}

uint64_t sec_read_cr4(void)
{
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    return cr4;
}

uint64_t sec_read_cr0(void)
{
    uint64_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    return cr0;
}

uint32_t sec_scrub_ram(void)
{
    uint32_t scrubbed = 0;
    for (uintptr_t frame = 0; frame < tpmm_total_mem() / PAGE_SIZE; frame++) {
        uintptr_t phys = frame * PAGE_SIZE;
        if (phys >= 0x40000000u)          /* only the mapped first 1 GiB */
            break;
        if (tpmm_frame_used(phys))
            continue;
        tmemset((void *)(uintptr_t)VMM_PHYS_TO_VIRT(phys), 0, PAGE_SIZE);
        scrubbed++;
    }
    return scrubbed;
}
