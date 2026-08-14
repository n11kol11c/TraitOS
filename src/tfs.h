#ifndef TFS_H
#define TFS_H

#include <stdint.h>
#include <stddef.h>

#define TFS_FILE 0
#define TFS_DIR  1

typedef struct tfs_node tfs_node_t;

/* procfs/sysfs nodes generate their content on read instead of storing it. */
typedef void (*tfs_generator)(tfs_node_t *node, char *buf, size_t bufsz);

struct tfs_node {
    char name[64];
    uint8_t type;              /* TFS_FILE | TFS_DIR */
    size_t size;               /* file length in bytes */
    char *data;                /* file payload (heap), NULL otherwise */
    tfs_generator gen;         /* virtual content, NULL otherwise */
    struct tfs_node *parent;
    struct tfs_node *children; /* dir: linked list of children */
    struct tfs_node *next;     /* sibling link */
};

void          tfs_init(void);
tfs_node_t   *tfs_root(void);
tfs_node_t   *tfs_lookup(const char *path);
tfs_node_t   *tfs_mkdir(const char *path);   /* creates missing parents */
tfs_node_t   *tfs_touch(const char *path);
int           tfs_write(tfs_node_t *node, const char *data, size_t len);
int           tfs_rm(tfs_node_t *node);
void          tfs_list(tfs_node_t *dir, void (*cb)(tfs_node_t *));
tfs_node_t   *tfs_add_virtual(tfs_node_t *dir, const char *name,
                              tfs_generator gen);
void          tfs_mount(const char *name, tfs_node_t *node);
void          tfs_mount_list(void (*cb)(const char *name, tfs_node_t *node));

/* tarfs: unpack a ustar archive into the ramfs tree. */
int tfs_unpack_tar(const uint8_t *tar, size_t len);

/* Load the Multiboot2 initrd module and unpack it into ramfs. */
int tfs_load_initrd(uintptr_t mbi);

/* procfs / sysfs subtrees. */
void tfs_procfs_init(void);
void tfs_sysfs_init(void);

#endif
