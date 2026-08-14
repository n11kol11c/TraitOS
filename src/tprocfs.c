#include "tfs.h"

#include "tstring.h"
#include "timer.h"
#include "tpmm.h"
#include "tmalloc.h"

#include <stdint.h>

static void gen_uptime(tfs_node_t *node, char *buf, size_t bufsz)
{
    node->size = (size_t)tsprintf(buf, bufsz,
        "uptime: %u seconds (%u ticks)\n",
        (uint32_t)(timer_ticks() / 100), (uint32_t)timer_ticks());
}

static void gen_version(tfs_node_t *node, char *buf, size_t bufsz)
{
    node->size = (size_t)tsprintf(buf, bufsz,
        "TraitOS v0.9.0 (x86_64, Multiboot2)\n");
}

static void gen_meminfo(tfs_node_t *node, char *buf, size_t bufsz)
{
    node->size = (size_t)tsprintf(buf, bufsz,
        "MemTotal:     %u KiB\n"
        "MemAvailable: %u KiB\n"
        "Frames free:  %u\n"
        "Frames used:  %u\n",
        (uint32_t)(tpmm_total_mem() >> 10),
        (uint32_t)(tpmm_available_mem() >> 10),
        tpmm_free_frames(), tpmm_used_frames());
}

static void gen_heapinfo(tfs_node_t *node, char *buf, size_t bufsz)
{
    node->size = (size_t)tsprintf(buf, bufsz,
        "Heap mapped: %u KiB\n"
        "Heap used:   %u KiB\n"
        "Blocks:      %u\n",
        (uint32_t)(tmalloc_total() >> 10),
        (uint32_t)(tmalloc_used() >> 10), tmalloc_blocks());
}

void tfs_procfs_init(void)
{
    tfs_node_t *proc = tfs_mkdir("/proc");
    if (!proc)
        return;
    tfs_add_virtual(proc, "uptime", gen_uptime);
    tfs_add_virtual(proc, "version", gen_version);
    tfs_add_virtual(proc, "meminfo", gen_meminfo);
    tfs_add_virtual(proc, "heapinfo", gen_heapinfo);
    tfs_mount("procfs", proc);
}
