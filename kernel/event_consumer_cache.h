/* Cache d_names and settings (in flags) for
 * the given struct path. Note that in rare cases
 * wrong (older) paths may be returned:
 * 1. Currently we hold no referecne on the struct path. If the
 *    memory adress is reused, we return the old path.
 * 2. On a hit, we do not resolve the path again, which might
 *    have changed meanwhile.
 * However, the cache is invalidated after a short time and
 * at least the filename is always correct.
 *
 */

#pragma once

#include "shournalk_global.h"

#include <linux/path.h>
#include <linux/limits.h>

// For consumer_cache_entry.flags
enum
{
    DIRCACHE_W_OFF      = 1 << 0,
    DIRCACHE_R_OFF      = 1 << 1,
    DIRCACHE_SCRIPT_OFF = 1 << 2,
};


struct consumer_cache_entry {
    struct path dir; /* WARNING - do not dereference */
    char* dirname;
    int dirname_len;
    int flags; // e.g. DIRCACHE_W_OFF
    unsigned long __cache_invalid_jiffy;
    char __dirname_buf[PATH_MAX];
};

struct consumer_cache {
    struct consumer_cache_entry _last_entry;
};


void consumer_cache_init(struct consumer_cache*);

struct consumer_cache_entry* consumer_cache_find(
        struct consumer_cache*, struct vfsmount*, struct dentry*,
        bool* existed);

