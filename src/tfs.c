#include "tfs.h"

#include "tmalloc.h"
#include "tstring.h"

#include <stdint.h>

#define MAX_NAME 64
#define MAX_MOUNTS 8

static tfs_node_t root = {
    .name = "/",
    .type = TFS_DIR,
    .parent = &root,
};

struct mount_entry {
    char name[32];
    tfs_node_t *node;
};

static struct mount_entry mounts[MAX_MOUNTS];
static int nmounts = 0;

void tfs_init(void)
{
    nmounts = 0;
}

tfs_node_t *tfs_root(void)
{
    return &root;
}

void tfs_mount(const char *name, tfs_node_t *node)
{
    if (nmounts >= MAX_MOUNTS || !node)
        return;
    tstrncpy(mounts[nmounts].name, name, sizeof mounts[nmounts].name);
    mounts[nmounts].node = node;
    nmounts++;
}

void tfs_mount_list(void (*cb)(const char *name, tfs_node_t *node))
{
    for (int i = 0; i < nmounts; i++)
        cb(mounts[i].name, mounts[i].node);
}

static tfs_node_t *find_child(tfs_node_t *dir, const char *name, size_t len)
{
    for (tfs_node_t *c = dir->children; c; c = c->next)
        if (tstrlen(c->name) == len && tstrncmp(c->name, name, len) == 0)
            return c;
    return NULL;
}

static tfs_node_t *alloc_node_len(const char *name, size_t len, uint8_t type)
{
    tfs_node_t *n = (tfs_node_t *)tmalloc(sizeof(tfs_node_t));
    if (!n)
        return NULL;
    tmemset(n, 0, sizeof *n);
    if (len >= MAX_NAME)
        len = MAX_NAME - 1;
    tmemcpy(n->name, name, len);
    n->name[len] = '\0';
    n->type = type;
    return n;
}

static tfs_node_t *alloc_node(const char *name, uint8_t type)
{
    return alloc_node_len(name, tstrlen(name), type);
}

/* Walk `path` from `start`. Segments that already exist are traversed;
 * missing segments are created as directories. If the final segment does
 * not exist, it is returned in `leaf` and the parent dir is returned. */
static tfs_node_t *resolve_create(tfs_node_t *start, const char *path,
                                  char *leaf, size_t leafsz)
{
    tfs_node_t *cur = start;
    const char *p = path;

    while (*p) {
        while (*p == '/')
            p++;
        if (!*p)
            break;

        const char *seg = p;
        while (*p && *p != '/')
            p++;
        size_t len = (size_t)(p - seg);

        if (len == 1 && seg[0] == '.')
            continue;
        if (len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (cur->parent)
                cur = cur->parent;
            continue;
        }

        if (cur->type != TFS_DIR)
            return NULL;

        tfs_node_t *child = find_child(cur, seg, len);
        if (!child) {
            if (!*p) {                       /* final segment: the leaf */
                if (len >= leafsz)
                    return NULL;
                tmemset(leaf, 0, leafsz);
                tmemcpy(leaf, seg, len);
                return cur;
            }
            child = alloc_node_len(seg, len, TFS_DIR);
            if (!child)
                return NULL;
            child->parent = cur;
            child->next = cur->children;
            cur->children = child;
        }
        cur = child;
    }
    return NULL;                             /* path ended at an existing node */
}

tfs_node_t *tfs_lookup(const char *path)
{
    tfs_node_t *cur = &root;
    const char *p = path;

    while (*p) {
        while (*p == '/')
            p++;
        if (!*p)
            break;

        const char *seg = p;
        while (*p && *p != '/')
            p++;
        size_t len = (size_t)(p - seg);

        if (len == 1 && seg[0] == '.')
            continue;
        if (len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (cur->parent)
                cur = cur->parent;
            continue;
        }
        if (cur->type != TFS_DIR)
            return NULL;
        cur = find_child(cur, seg, len);
        if (!cur)
            return NULL;
    }
    return cur;
}

static tfs_node_t *make_node(const char *path, uint8_t type)
{
    char leaf[MAX_NAME];
    tfs_node_t *parent = resolve_create(&root, path, leaf, sizeof leaf);
    if (!parent)
        return NULL;
    if (find_child(parent, leaf, tstrlen(leaf)))
        return NULL;

    tfs_node_t *n = alloc_node(leaf, type);
    if (!n)
        return NULL;
    n->parent = parent;
    n->next = parent->children;
    parent->children = n;
    return n;
}

tfs_node_t *tfs_mkdir(const char *path)
{
    return make_node(path, TFS_DIR);
}

tfs_node_t *tfs_touch(const char *path)
{
    return make_node(path, TFS_FILE);
}

tfs_node_t *tfs_add_virtual(tfs_node_t *dir, const char *name, tfs_generator gen)
{
    if (!dir || dir->type != TFS_DIR)
        return NULL;
    tfs_node_t *n = alloc_node(name, TFS_FILE);
    if (!n)
        return NULL;
    n->gen = gen;
    n->parent = dir;
    n->next = dir->children;
    dir->children = n;
    return n;
}

int tfs_write(tfs_node_t *node, const char *data, size_t len)
{
    if (!node || node->type != TFS_FILE)
        return -1;

    char *buf = NULL;
    if (len > 0) {
        buf = (char *)tmalloc(len);
        if (!buf)
            return -1;
        tmemcpy(buf, data, len);
    }
    if (node->data)
        tfree(node->data);
    node->data = buf;
    node->size = len;
    return 0;
}

static void free_children(tfs_node_t *node)
{
    tfs_node_t *c = node->children;
    while (c) {
        tfs_node_t *next = c->next;
        free_children(c);
        if (c->data)
            tfree(c->data);
        tfree(c);
        c = next;
    }
    node->children = NULL;
}

int tfs_rm(tfs_node_t *node)
{
    if (!node || node == &root)
        return -1;

    tfs_node_t **pp = &node->parent->children;
    while (*pp && *pp != node)
        pp = &(*pp)->next;
    if (*pp)
        *pp = node->next;

    free_children(node);
    if (node->data)
        tfree(node->data);
    tfree(node);
    return 0;
}

void tfs_list(tfs_node_t *dir, void (*cb)(tfs_node_t *))
{
    if (!dir || dir->type != TFS_DIR)
        return;
    for (tfs_node_t *c = dir->children; c; c = c->next)
        cb(c);
}
