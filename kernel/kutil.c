
#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <linux/uio.h>
#include <linux/pagemap.h>
#include <linux/pipe_fs_i.h>
#include <linux/rcupdate.h>

#include "kutil.h"

#ifdef KVMALLOC_BACKPORT
#include <linux/vmalloc.h>

void *_kvmalloc_node_backport(size_t size, gfp_t flags, int node)
{
    gfp_t kmalloc_flags = flags;
    void *ret;

    /*
     * vmalloc uses GFP_KERNEL for some internal allocations (e.g page tables)
     * so the given set of flags has to be compatible.
     */
    WARN_ON_ONCE((flags & GFP_KERNEL) != GFP_KERNEL);

    /*
     * Make sure that larger requests are not too disruptive - no OOM
     * killer and no allocation failure warnings as we have a fallback
     */
    if (size > PAGE_SIZE)
        kmalloc_flags |= __GFP_NORETRY | __GFP_NOWARN;

    ret = kmalloc_node(size, kmalloc_flags, node);

    /*
     * It doesn't really make sense to fallback to vmalloc for sub page
     * requests
     */
    if (ret || size <= PAGE_SIZE)
        return ret;

    return __vmalloc(size, flags | __GFP_HIGHMEM, PAGE_KERNEL);
}

#endif


/// Resolve the pathname of a regular, *not* deleted file.
/// @param file The file to resolve the pathname of
/// @param resolved_pathname a buffer of at least PATH_MAX size.
char* resolve_reg_filepath(struct files_struct *files,
                           struct file *file,
                           char * buf){
    // see also proc_fd_link and
    // https://stackoverflow.com/a/8250940/7015849
    struct path *path;
    char* pathname;

    spin_lock(&files->file_lock);
    if (!file || !S_ISREG(file_inode(file)->i_mode) ||
            file_inode(file)->i_nlink == 0) {
        spin_unlock(&files->file_lock);
        return NULL;
    }

    path = &file->f_path;
    path_get(path);
    spin_unlock(&files->file_lock);

    pathname = d_path(path, buf, PATH_MAX);
    path_put(path);

    if (IS_ERR(pathname)) {
        return NULL;
    }
    return pathname;
}


ssize_t kutil_kernel_write(struct file *file, const void *buf, size_t count, loff_t *pos){
    ssize_t ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    ret = kernel_write(file, buf, count, pos);
#else
    mm_segment_t fs_save;

    fs_save = get_fs();
    set_fs(get_ds());
    ret = vfs_write(file, buf, count, pos);
    set_fs(fs_save);
#endif
    return ret;
}


ssize_t kutil_kernel_write_locked(struct file * file, const void *buf, size_t count)
{
    ssize_t ret;

    mutex_lock(&file->f_pos_lock);

    ret = kutil_kernel_write(file, buf, count, &file->f_pos);

    mutex_unlock(&file->f_pos_lock);
    return ret;
}


ssize_t kutil_kernel_read_locked(struct file *file, void *buf, size_t count){
    ssize_t ret;

    mutex_lock(&file->f_pos_lock);

    ret = kutil_kernel_read(file, buf, count, &file->f_pos);

    mutex_unlock(&file->f_pos_lock);
    return ret;
}

/// Try to read from file without disturbing the page cache.
/// Eventually we should use mmap and something like
/// MADV_FREE instead (zap_page_range)?
/// Or re-suggest a similar approach Jens Axboe proposed in Dec 2019, e.g.
///     [PATCH 1/5] fs: add read support for RWF_UNCACHED
///     https://lwn.net/Articles/807519/
/// Imaginable is e.g. a preadv2-flag RWF_CACHEFRIENDLY which does not
/// call mark_page_accessed() when a page is read.
///
/// Anyhow, here we trick mm/filemap.c:filemap_read() (v5.12-rc5-3-g1e43c377a79f)
/// into *not* calling mark_page_accessed by assigning ra->prev_pos to the current
/// pos(ition):
/// if (iocb->ki_pos >> PAGE_SHIFT !=
///     ra->prev_pos >> PAGE_SHIFT)
///     mark_page_accessed(pvec.pages[0]);
/// Note that this only works, if we read one page, that's why we
/// call kernel_read multiple times, if necessary.
///
/// Another approach might be to clear the page-reference-bit afterwards,
/// but I'm not completely sure that this is legal..:
/// struct address_space *mapping = file->f_mapping;
/// struct page* page = find_get_entry(mapping, *pos / PAGE_SIZE);
/// kernel_read();
/// ClearPageReferenced(page);
ssize_t
kutil_kernel_read_cachefriendly(struct file *file, void *buf, size_t count, loff_t *pos){
    ssize_t read_size_total = 0;
    struct file_ra_state *ra = &file->f_ra;

    while(1){
        ssize_t ret;
        ssize_t current_count;
        size_t count_to_page_end = round_up(*pos, PAGE_SIZE) - *pos;
        if(count_to_page_end  == 0){
            // At page-start.
            count_to_page_end = PAGE_SIZE;
        }

        current_count = min(count - read_size_total, count_to_page_end);
        ra->prev_pos = *pos;
        ret = kutil_kernel_read(file, buf, current_count, pos);
        if(unlikely(ret < 0)){
            return ret;
        }
        read_size_total += ret;
        if(ret < current_count || read_size_total >= (ssize_t)count){
            // EOF or everything read
            return read_size_total;
        }
        buf += ret;
    }
}


void kutil_take_name_snapshot(struct kutil_name_snapshot* snapshot,
                              struct dentry *dentry){

    // see lib/vsprintf.c:dentry_name and fs/dcache.c:dentry_cmp
    // -> apparently no locking is required for
    // reading d_name.name, because:
    // "dentry name is guaranteed to be properly terminated with a NUL byte".
    // However, linux/dache.h:take_dentry_name_snapshot
    // does locking (probably for consistent hash/len?).
    // (unsafe: snapshot->name = dentry->d_name;)

    const unsigned char *name;

    rcu_read_lock();
    name = READ_ONCE(dentry->d_name.name);
    strncpy((char*)snapshot->inline_name,
            (const char*)name, sizeof (snapshot->inline_name) - 1);
    rcu_read_unlock();

    // interface compatibility with struct name_snapshot from dcache.h
    snapshot->name.name = snapshot->inline_name;
    snapshot->name.len = (u32)strnlen((const char*)snapshot->name.name,
                                 sizeof (snapshot->inline_name));
    if(unlikely(snapshot->name.len == sizeof (snapshot->inline_name))){
        snapshot->inline_name[sizeof (snapshot->inline_name) - 1] = '\0';
        pr_warn_once("Bug! Unterminated filename found: %s", snapshot->name.name);
    }
}

// interface compatibility with struct name_snapshot
void kutil_release_name_snapshot(struct kutil_name_snapshot *name __attribute__ ((unused)))
{}


#ifdef kutil_BACKPORT_USE_MM

#include <linux/mmu_context.h>

// declare as weak to satisfy compiler. However,
// one of use_mm or kthread_use_mm _must_ be defined (by kernel).
void use_mm(struct mm_struct *mm) __attribute__((weak));
void unuse_mm(struct mm_struct *mm) __attribute__((weak));
void kthread_use_mm(struct mm_struct*) __attribute__((weak));
void kthread_unuse_mm(struct mm_struct*) __attribute__((weak));


void kutil_use_mm(struct mm_struct *mm) {
    if(use_mm)
        use_mm(mm);
    else if(kthread_use_mm)
        kthread_use_mm(mm);
    else
        pr_warn_once("kthread_use_mm and use_mm not defined - please report");
}

void kutil_unuse_mm(struct mm_struct *mm) {
    if(unuse_mm)
        unuse_mm(mm);
    else if(kthread_unuse_mm)
        kthread_unuse_mm(mm);
    else
        pr_warn_once("kthread_unuse_mm and unuse_mm not defined - please report");
}

#endif // kutil_BACKPORT_USE_MM


// see commit cead18552660702a4a46f58e65188fe5f36e9dfe
// declare as weak to satisfy compiler. However,
// one of complete_and_exit and kthread_complete_and_exit
// _must_ be defined (by kernel).
void complete_and_exit(struct completion *comp, long code)__attribute__((weak));
void kthread_complete_and_exit(struct completion *comp, long code)__attribute__((weak));

void kutil_kthread_exit(struct completion *comp, long code){
    if(complete_and_exit)
        complete_and_exit(comp, code);
    else if(kthread_complete_and_exit)
        kthread_complete_and_exit(comp, code);
    pr_err("Failed to stop kernel thread. Please unload this module "
           "immediatly and report this fatal bug.");
}


#ifdef RCU_WORK_BACKPORT

static void rcu_work_rcufn(struct rcu_head *rcu)
{
    struct rcu_work *rwork = container_of(rcu, struct rcu_work, rcu);
    queue_work(rwork->wq, &rwork->work);
}

bool queue_rcu_work(struct workqueue_struct *wq, struct rcu_work *rwork)
{
    rwork->wq = wq;
    call_rcu(&rwork->rcu, rcu_work_rcufn);
    return true;
}

#endif // RCU_WORK_BACKPORT

#ifdef GET_MEMCG_FROM_MM_BACKPORT

struct mem_cgroup *_get_mem_cgroup_from_mm_backport(struct mm_struct *mm)
{
    struct mem_cgroup *memcg = NULL;

    if (unlikely(!mm))
        return NULL;

    rcu_read_lock();
    do {
        memcg = mem_cgroup_from_task(rcu_dereference(mm->owner));
        if (unlikely(!memcg))
            break;

    } while (!css_tryget_online(&memcg->css));
    rcu_read_unlock();
    return memcg;
}

#endif // GET_MEMCG_FROM_MM_BACKPORT



