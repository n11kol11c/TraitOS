/* Host-side smoke test for the RAM filesystem. No emulator required.
 *
 * Compiles the fs modules against the host libc with tiny stubs for the
 * kernel-only symbols, then unpacks the real boot/ramfs.img and verifies
 * the tree. Run via `make smoke`.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tfs.h"
#include "tstring.h"
#include "tvmm.h"

/* ---- host stubs for kernel-only symbols ---- */
void *tmalloc(size_t n) { return malloc(n); }
void  tfree(void *p) { free(p); }
void *trealloc(void *p, size_t n) { return realloc(p, n); }
uint64_t tmalloc_total(void) { return 0; }
uint64_t tmalloc_used(void) { return 0; }
uint32_t tmalloc_blocks(void) { return 0; }
uint32_t timer_ticks(void) { return 0; }
uint64_t tpmm_total_mem(void) { return 0; }
uint64_t tpmm_available_mem(void) { return 0; }
uint32_t tpmm_free_frames(void) { return 0; }
uint32_t tpmm_used_frames(void) { return 0; }
int vmm_map_page(uintptr_t v, uintptr_t p, uint64_t flags) { (void)v; (void)p; (void)flags; return 0; }
void vmm_unmap_page(uintptr_t v) { (void)v; }
char _kernel_virtual_start[] = "";
char _kernel_phys_start[] = "";
char _kernel_phys_end[] = "";

static int depth;
static int errors;

static void pr_indent(void) { for (int i = 0; i < depth; i++) printf("  "); }

static void walk(tfs_node_t *n)
{
    pr_indent();
    printf("%s%s\n", n->name, n->type == TFS_DIR ? "/" : "");
    if (n->type == TFS_DIR) {
        depth++;
        tfs_list(n, walk);
        depth--;
    }
}

static void cat(const char *path)
{
    tfs_node_t *n = tfs_lookup(path);
    if (!n) { printf("CAT %s: NOT FOUND\n", path); errors++; return; }
    if (n->gen) {
        char buf[256];
        n->gen(n, buf, sizeof buf);
        printf("--- cat %s (%u bytes) ---\n", path, (unsigned)n->size);
        printf("%s", buf);
    } else if (n->data) {
        printf("--- cat %s (%u bytes) ---\n", path, (unsigned)n->size);
        printf("%.*s", (int)n->size, n->data);
    } else {
        printf("--- cat %s (empty) ---\n", path);
    }
}

static void expect(const char *path, const char *what, int cond)
{
    if (!cond) {
        printf("%s: %s FAILED\n", what, path);
        errors++;
    }
}

int main(void)
{
    FILE *f = fopen("boot/ramfs.img", "rb");
    if (!f) {
        printf("cannot open boot/ramfs.img - run 'make smoke' (builds it first)\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *img = malloc((size_t)len);
    if (fread(img, 1, (size_t)len, f) != (size_t)len) {
        printf("short read\n");
        return 1;
    }
    fclose(f);

    tfs_init();
    tfs_procfs_init();
    tfs_sysfs_init();

    printf("unpacking %ld bytes from boot/ramfs.img\n", len);
    if (tfs_unpack_tar(img, len) != 0) {
        printf("unpack failed\n");
        return 1;
    }

    printf("\n-- initrd tree --\n");
    walk(tfs_lookup("/"));

    expect("/README.txt", "cat", tfs_lookup("/README.txt") != NULL);
    expect("/etc/hostname", "cat", tfs_lookup("/etc/hostname") != NULL);
    expect("/etc/passwd", "cat", tfs_lookup("/etc/passwd") != NULL);
    expect("/usr/bin/hello", "cat", tfs_lookup("/usr/bin/hello") != NULL);
    expect("/var/log/boot.log", "cat", tfs_lookup("/var/log/boot.log") != NULL);
    cat("/README.txt");
    cat("/etc/hostname");
    cat("/etc/passwd");
    cat("/usr/bin/hello");
    cat("/var/log/boot.log");

    printf("\n-- procfs/sysfs --\n");
    expect("/proc/version", "cat", tfs_lookup("/proc/version") != NULL);
    expect("/proc/uptime", "cat", tfs_lookup("/proc/uptime") != NULL);
    expect("/sys/kernel", "cat", tfs_lookup("/sys/kernel") != NULL);
    cat("/proc/version");
    cat("/proc/uptime");
    cat("/sys/kernel");

    printf("\n-- write test --\n");
    tfs_node_t *f2 = tfs_touch("/etc/traitos.conf");
    expect("/etc/traitos.conf", "touch", f2 != NULL);
    const char *payload = "key=value\nhello=world\n";
    expect("/etc/traitos.conf", "write",
           f2 != NULL && tfs_write(f2, payload, strlen(payload)) == 0);
    cat("/etc/traitos.conf");

    printf("\n-- rm test --\n");
    tfs_node_t *rmf = tfs_lookup("/etc/traitos.conf");
    expect("/etc/traitos.conf", "rm", tfs_rm(rmf) == 0);
    expect("/etc/traitos.conf", "rm removed", tfs_lookup("/etc/traitos.conf") == NULL);
    cat("/etc/hostname");

    printf("\n-- deep mkdir -p --\n");
    expect("/a/b/c/d", "mkdir -p", tfs_mkdir("/a/b/c/d") != NULL);
    expect("/a/b/c/d", "lookup", tfs_lookup("/a/b/c/d") != NULL);
    expect("/a/b/c/d/e.txt", "touch", tfs_touch("/a/b/c/d/e.txt") != NULL);
    cat("/a/b/c/d/e.txt");

    printf("\n%s (%d errors)\n", errors ? "SMOKE TEST FAILED" : "SMOKE TEST PASSED", errors);
    return errors ? 1 : 0;
}
