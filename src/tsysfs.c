#include "tfs.h"

#include "tstring.h"
#include "tpmm.h"

#include <stdint.h>

extern char _kernel_virtual_start[];
extern char _kernel_phys_start[];
extern char _kernel_phys_end[];

static void gen_memory(tfs_node_t *node, char *buf, size_t bufsz)
{
    node->size = (size_t)tsprintf(buf, bufsz,
        "Total:     %u MiB\n"
        "Available: %u MiB\n"
        "Free:      %u frames\n",
        (uint32_t)(tpmm_total_mem() >> 20),
        (uint32_t)(tpmm_available_mem() >> 20), tpmm_free_frames());
}

static void gen_frames(tfs_node_t *node, char *buf, size_t bufsz)
{
    node->size = (size_t)tsprintf(buf, bufsz,
        "free: %u\n"
        "used: %u\n",
        tpmm_free_frames(), tpmm_used_frames());
}

static void gen_kernel(tfs_node_t *node, char *buf, size_t bufsz)
{
    node->size = (size_t)tsprintf(buf, bufsz,
        "virtual: %p\n"
        "phys:    %p - %p\n"
        "heap:    %p\n",
        _kernel_virtual_start,
        (void *)(uintptr_t)_kernel_phys_start,
        (void *)(uintptr_t)_kernel_phys_end,
        (void *)0xFFFFFFFFC0000000ULL);
}

void tfs_sysfs_init(void)
{
    tfs_node_t *sys = tfs_mkdir("/sys");
    if (!sys)
        return;
    tfs_add_virtual(sys, "memory", gen_memory);
    tfs_add_virtual(sys, "frames", gen_frames);
    tfs_add_virtual(sys, "kernel", gen_kernel);
    tfs_mount("sysfs", sys);
}
