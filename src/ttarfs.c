#include "tfs.h"

#include "tstring.h"
#include "tvmm.h"

#include <stdint.h>

/* Multiboot2 module tag (type 3) */
#define MB2_TAG_MODULE 3

/* Virtual scratch window where initrd pages are mapped before parsing. */
#define INITRD_SCRATCH 0xFFFFFFFE80000000ULL

/* Parse a ustar numeric field (octal, or base-256 if the high bit is set). */
static size_t tar_field(const char *field, size_t n)
{
    if (n == 0)
        return 0;
    if ((unsigned char)field[0] & 0x80) {      /* base-256 (GNU) */
        size_t v = (unsigned char)field[0] & 0x7F;
        for (size_t i = 1; i < n; i++)
            v = (v << 8) | (unsigned char)field[i];
        return v;
    }
    size_t v = 0;
    for (size_t i = 0; i < n; i++) {
        char c = field[i];
        if (c < '0' || c > '7')
            break;
        v = (v << 3) + (size_t)(c - '0');
    }
    return v;
}

static void strip_trailing_slash(char *name)
{
    size_t l = tstrlen(name);
    while (l > 1 && name[l - 1] == '/')
        name[--l] = '\0';
}

int tfs_unpack_tar(const uint8_t *tar, size_t len)
{
    size_t off = 0;

    while (off + 512 <= len) {
        const uint8_t *h = tar + off;

        if (h[0] == '\0')                       /* end of archive */
            break;

        char typeflag = (char)h[156];
        size_t size = tar_field((const char *)h + 124, 12);
        const char *prefix = (const char *)h + 345;
        const char *name = (const char *)h;

        char full[128];
        if (prefix[0] != '\0')
            tsprintf(full, sizeof full, "%s/%s", prefix, name);
        else
            tstrncpy(full, name, sizeof full);
        strip_trailing_slash(full);

        const uint8_t *data = tar + off + 512;
        size_t recs = (size + 511) / 512;

        switch (typeflag) {
        case '5':                               /* directory */
            if (full[0])
                tfs_mkdir(full);
            break;
        case '0':                               /* regular file */
        case '\0':
            if (full[0]) {
                tfs_node_t *f = tfs_touch(full);
                if (f && size > 0)
                    tfs_write(f, (const char *)data, size);
            }
            break;
        default:                                /* hard/soft links, pax, ... */
            break;                              /* skip content */
        }

        off += (recs + 1) * 512;
    }
    return 0;
}

int tfs_load_initrd(uintptr_t mbi)
{
    uintptr_t p = mbi + 8;                     /* skip total_size + reserved */
    uintptr_t end = mbi + *(uint32_t *)mbi;

    while (p + 8 <= end) {
        uint32_t type = *(uint32_t *)p;
        uint32_t size = *(uint32_t *)(p + 4);
        if (type == 0)
            break;

        if (type == MB2_TAG_MODULE) {
            uint32_t start = *(uint32_t *)(p + 8);
            uint32_t stop = *(uint32_t *)(p + 12);
            size_t len = (size_t)(stop - start);
            size_t pages = (len + 4095) / 4096;

            /* GRUB loaded the initrd at an arbitrary physical address; map
             * its pages into a scratch window before parsing. */
            for (size_t i = 0; i < pages; i++) {
                if (vmm_map_page(INITRD_SCRATCH + i * 4096,
                                 start + i * 4096, VMM_PAGE_WRITE) != 0) {
                    for (size_t j = 0; j < i; j++)
                        vmm_unmap_page(INITRD_SCRATCH + j * 4096);
                    return -1;
                }
            }

            int r = (len > 0)
                        ? tfs_unpack_tar((const uint8_t *)INITRD_SCRATCH, len)
                        : 0;
            for (size_t i = 0; i < pages; i++)
                vmm_unmap_page(INITRD_SCRATCH + i * 4096);
            return r;
        }
        p += (size + 7) & ~(uintptr_t)7;
    }
    return 0;                                   /* no initrd module */
}
