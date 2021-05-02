
#pragma once

#include "shournalk_global.h"

#include <linux/hashtable.h>
#include <linux/mutex.h>


#define KPATHTREE_BITS 6
#define KPATHTREE_MAX_SIZE (1 << KPATHTREE_BITS)

#define __KPATHTREE_INITIALIZER(treename) \
    { .n_paths = 0 \
    , .__n_path_sizes = 0 \
    , .__is_init = true \
    , .lock = __MUTEX_INITIALIZER(treename.lock) \
    , .path_table = { [0 ... ((1 << (KPATHTREE_BITS)) - 1)] = HLIST_HEAD_INIT } }

struct kpathtree {
    DECLARE_HASHTABLE(path_table, KPATHTREE_BITS);
    struct mutex lock;
    int n_paths; /* number of paths alreay added */
    int __path_sizes[KPATHTREE_MAX_SIZE];
    int __n_path_sizes;
    bool __is_init;
};


struct kpathtree* kpathtree_create(void);
void kpathtree_free(struct kpathtree* pathtree);

void kpathtree_init(struct kpathtree* pathtree);
void kpathtree_cleanup(struct kpathtree* pathtree);

long kpathtree_add(struct kpathtree* pathtree, const char* path, int path_len);
bool kpathtree_is_subpath(struct kpathtree* pathtree, const char* path,
                          int path_len, bool allow_equals);
