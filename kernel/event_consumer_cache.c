
#include <linux/jiffies.h>
#include <linux/dcache.h>

#include "event_consumer_cache.h"
#include "kutil.h"

// stolen from fs/proc/base.c:do_proc_readlink
static inline size_t
d_path_len(const char* buf, size_t buflen, const char* pathname){
    return buf + buflen - 1 - pathname;
}

static void __cache_entry_init(struct consumer_cache_entry* e){
    e->dir.mnt = NULL;
    e->dir.dentry = NULL;
    e->dirname = NULL;
    e->dirname_len = 0;
    e->flags = 0;
    e->__cache_invalid_jiffy = 0;
}

static bool __cache_entry_hit(const struct consumer_cache_entry*e,
                              const struct vfsmount *mnt,
                              const struct dentry *dentry){
    return  dentry == e->dir.dentry  &&
            mnt    == e->dir.mnt     &&
            time_is_after_jiffies(e->__cache_invalid_jiffy);

}


static bool __append_dname_to_parent(struct consumer_cache_entry* parent,
                                     struct qstr* dname ){
    if(parent->dirname_len + dname->len >= sizeof(parent->__dirname_buf)){
        pr_devel("path-buffer too small for %s/%s",
                 parent->dirname, dname->name);
        return false;
    }
    if(parent->dirname != parent->__dirname_buf){
        // d_path prepends backwards, to make sure we have the full buffer-length
        // move to the front of our buffer
        memmove(parent->__dirname_buf, parent->dirname, parent->dirname_len + 1);
        parent->dirname = parent->__dirname_buf;
    }

    if(parent->dirname_len > 1){
        // not the root node
        parent->dirname[parent->dirname_len] = '/';
        parent->dirname_len++;
    }
    memcpy(parent->dirname + parent->dirname_len,
            (const char*)dname->name, dname->len + 1);
    parent->dirname_len += dname->len;

    return true;
}


void consumer_cache_init(struct consumer_cache* c){
    __cache_entry_init(&c->_last_entry);
}


/// Try to find cached meta-data for the given directory
/// @param existed: set to true, if existed
/// @return the found or new entry or an ERROR_PTR on err. Note that
/// in rare cases the corresponding directory-path may be *wrong*, because
/// currently no reference on struct path is held!
struct consumer_cache_entry* consumer_cache_find(
        struct consumer_cache* c, struct vfsmount *mnt, struct dentry *dentry,
        bool* existed){
    // dentry is initialized NULL, so on first call we never return true
    struct consumer_cache_entry* e = &c->_last_entry;
    struct dentry* dparent;

    if(__cache_entry_hit(e, mnt, dentry)){
        *existed = true;
        return e;
    }
    dparent = READ_ONCE(dentry->d_parent);

    if(__cache_entry_hit(e, mnt, dparent)){
        bool append_success;
        struct kutil_name_snapshot name_snapshot;
        kutil_take_name_snapshot(&name_snapshot, dentry);
        append_success = __append_dname_to_parent(e, &name_snapshot.name);
        kutil_release_name_snapshot(&name_snapshot);
        if(unlikely(! append_success)){
            return ERR_PTR(-EDOM);
        }
        e->dir.dentry = dentry;
        // for now, set existed to false, because we don't know
        // whether child is e.g. an exclude-dir, if parent was so.
        *existed = false;
        return e;
    }

    *existed = false;

    // maybe_todo: hold a path_get reference for correctness (implications?)?
    e->dir.mnt = mnt;
    e->dir.dentry = dentry;
    e->dirname = d_path(&e->dir, e->__dirname_buf, PATH_MAX);
    if (IS_ERR(e->dirname)) {
        e->dir.dentry = NULL;
        pr_devel("failed to resolve pathname\n");
        // Dbg: print raw path in case d_path fail (why?)
        // pathname = dentry_path_raw(e->file->f_path.dentry, g_tmp_path, PATH_MAX);
        return (struct consumer_cache_entry*)e->dirname;
    }
    e->dirname_len = (int)(d_path_len(e->__dirname_buf, PATH_MAX, e->dirname));
    e->__cache_invalid_jiffy = jiffies + msecs_to_jiffies(5000);

    return e;
}

