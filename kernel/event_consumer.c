
#include <linux/compiler.h>
#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/fadvise.h>
#include <linux/kthread.h>
#include <linux/mmu_context.h>
#include <linux/splice.h>
#include <asm/uaccess.h>


#include "event_consumer.h"
#include "event_consumer_cache.h"
#include "event_queue.h"
#include "event_target.h"
#include "kutil.h"
#include "shournal_kio.h"
#include "shournalk_user.h"
#include "kpathtree.h"

#include "xxhash_common.h"

#define CONSUMER_CIRC_BUFSIZE (1 << 15)


static inline bool __path_is_hidden(const char* pathname, int path_len){
    return strnstr(pathname, "/.", path_len) != NULL;
}

static inline struct file *
__reopen_file_silent(const struct path *path, const struct cred * cred){
    return dentry_open(path,
                       O_RDONLY | O_NOATIME | FMODE_NONOTIFY,
                       cred);
}



#ifdef DEBUG
static void __dbg_print_event(struct event_target* t __attribute__ ((unused)),
                              struct close_event* ev,
                     const char* msg){

        const char* pathname;
        char* buf = (char*)(__get_free_page(GFP_KERNEL));
        if(! buf){
            pr_devel("__get_free_page failed!\n");
            return;
        }
        pathname = d_path(&ev->path, buf, PATH_MAX);
        if(IS_ERR(pathname)){
            pr_devel("%s failed to resolve pathname..\n", msg);
        } else {
            pr_devel("%s %s\n", msg, pathname);
        }
        free_page((ulong)buf);
}
#else
static void __dbg_print_event(struct event_target* t __attribute__ ((unused)),
                              struct close_event* ev __attribute__ ((unused)),
                              const char* msg __attribute__ ((unused))){}
#endif



static bool __write_to_target_file_safe(struct event_target* event_target,
                                      const void *buf, size_t count){
    const char* msg;
    ssize_t ret = shournal_kio_write(event_target->file, buf, count);
    if(likely(ret == (ssize_t)count)){
        return true;
    }

    WRITE_ONCE(event_target->ERROR, true);
    if(ret >= 0) {
        ret = EIO;
        msg = "not all bytes written";
    } else {
        ret = -ret;
        msg = "errno";
    }
    pr_debug("Failed to write to event target with parent pid %d - %s: %ld\n",
             event_target->caller_tsk->pid, msg, ret);
    event_target_write_result_to_user_ONCE(event_target, (int)ret);
    return false;
}

/// xxhash the passed file
static void __do_hash_file(struct partial_xxhash* part_hash,
                               struct file* file,
                               loff_t file_size,
                               const struct qstr* filename,
                               struct shournalk_close_event* user_event){
    struct partial_xxhash_result hash_result;
    long ret;
    kutil_WARN_DBG(file_size == 0, "file_size == 0");
    kutil_WARN_DBG(part_hash->chunksize == 0, "part_hash->chunksize == 0");

    part_hash->seekstep =
            file_size / part_hash->max_count_of_reads;
    if(unlikely(ret = partial_xxh_digest_file(file,
                                      part_hash,
                                      &hash_result))){
        pr_devel("failed to partial_hash file with %ld - %s\n", ret, filename->name);
        goto invalidate_hash;
    }

    if(unlikely(hash_result.count_of_bytes == 0)){
        // zero bytes read - file became empty in between?
        pr_devel("zero bytes read for previously non-empty file");
        goto invalidate_hash;
    }
    user_event->hash_is_null = false;
    user_event->hash = hash_result.hash;

    return;


invalidate_hash:
    user_event->hash = 0;
    user_event->hash_is_null = true;
}


/// rewrite user_event after having corrected the file content
/// size with the actual number of bytes written (should
/// happen rarely)
static bool
__correct_file_event_at_pos(struct event_target* t, loff_t pos,
                            struct shournalk_close_event* user_event){
    ssize_t ret;
    struct file* dest = t->file->__file;
    if( (ret = kutil_kernel_write(dest, user_event,
                       sizeof(struct shournalk_close_event),
                       &pos
                       )) !=  sizeof(struct shournalk_close_event)){
        WRITE_ONCE(t->ERROR, true);
        pr_debug("Failed to correct file content size for target %s, returned %ld\n",
                 t->file_init_path, ret);
        event_target_write_result_to_user_ONCE(t, (int)-ret);
        return false;
    }
    return true;
}


/// write size bytes from src to
/// our target file
static bool
__write_file_content(struct event_target* t,
                     struct file* src,
                     loff_t size,
                     struct shournalk_close_event* user_event){
    ssize_t ret;
    struct file* dest = t->file->__file;
    loff_t src_pos = 0;
    loff_t old_dst_pos;
    loff_t written_size;

    if(unlikely(! event_consumer_flush_target_file_safe(t))){
        return false;
    }

    file_start_write(dest);
    old_dst_pos = dest->f_pos;
    ret = do_splice_direct(src, &src_pos, dest, &dest->f_pos, size, 0);
    written_size = dest->f_pos - old_dst_pos;
    file_end_write(dest);

    if(unlikely(written_size != size)){
        // before having written the file content, the
        // close event was written, which we overwrite now.
        // seek back and correct the written bytes
        loff_t correct_pos = old_dst_pos - sizeof(struct shournalk_close_event);
        user_event->bytes = written_size;
        pr_debug("Only %lld of %lld bytes written - attempting "
                 "to correct this...\n", written_size, size);
        return __correct_file_event_at_pos(t, correct_pos, user_event);

    }
    return true;
}


static bool __do_log_file_event(struct event_target* t,
                                struct close_event* close_ev,
                                int event_flags,
                                struct qstr* filename,
                                bool store_whole_file,
                                struct consumer_cache_entry* directory,
                                struct path* last_directory){
    struct shournalk_close_event user_event;
    struct file* file = NULL;
    const struct inode* inode = close_ev->path.dentry->d_inode;

    if(current->mm && unlikely(current->mm->owner != t->caller_tsk)){
        WRITE_ONCE(t->ERROR, true);
        // See comment in event_consumer_thread_setup
        // for the rationale.
        pr_debug("mm->owner does not belong to pid %d "
                 "any more - most likely the caller died. "
                 "Event-logging was stopped.\n", t->caller_tsk->pid);
        event_target_write_result_to_user_ONCE(t, EREMCHG);

        return false;
    }


    user_event.flags = event_flags;
    user_event.mtime = kutil_get_mtime_sec(inode);
    user_event.size = inode->i_size;
    user_event.mode = inode->i_mode;
    user_event.hash_is_null = t->partial_hash.chunksize == 0 ||
                              unlikely(user_event.size == 0);

    if(! user_event.hash_is_null || store_whole_file){
        file = __reopen_file_silent(&close_ev->path, t->cred);
        if( unlikely(IS_ERR_OR_NULL(file))) {
            pr_devel("failed to reopen file %s\n", filename->name);
            user_event.hash_is_null = true;
            store_whole_file = false;
        } else {
            long ret;
            // maybe_todo: only set to random, if ! store_whole_file?
            ret = vfs_fadvise(file, 0,0, POSIX_FADV_RANDOM);
            if(ret){
                pr_devel("vfs_fadvise failed with %ld\n", ret);
            }

        }
    }
    user_event.bytes = (store_whole_file) ? user_event.size : 0;
    if(! user_event.hash_is_null){
        __do_hash_file(&t->partial_hash, file, user_event.size, filename, &user_event);
    }

    if(unlikely(! __write_to_target_file_safe(
                    t, &user_event,
                    sizeof (struct shournalk_close_event)))
            ){
        return false;
    }

    if(unlikely(store_whole_file)){
        if(__write_file_content(t, file, user_event.bytes, &user_event)){
            t->stored_files_count++;
        }
    }
    if(! IS_ERR_OR_NULL(file)){
        fput(file);
    }

    // Only write directory path, if not written before
    if(! path_equal(last_directory, &directory->dir)){
        __write_to_target_file_safe(t, directory->dirname, directory->dirname_len);
        __write_to_target_file_safe(t, "/", 1);
        *last_directory = directory->dir;
    }
    __write_to_target_file_safe(t, filename->name, filename->len + 1);

    return true;
}


static void
__handle_read_event(struct event_target* t,
                    struct close_event* close_ev,
                    bool may_write){
    const struct shounalk_settings* sets = &t->settings;
    bool general_discard;
    bool store_discard;
    struct kutil_name_snapshot name_snapshot;
    struct qstr* filename;
    const struct inode* inode = close_ev->path.dentry->d_inode;
    int path_is_hidden = -1;
    struct path* path = &close_ev->path;
    struct consumer_cache_entry* d_ent;
    bool cache_entry_existed;

    t->r_examined_count++;

    kutil_WARN_DBG(! t->r_enable, "! t->r_enable");

    if(inode->i_nlink == 0){
        t->r_deleted_count++;
        pr_devel("ignore deleted file\n");
        return;
    }
    // Where possible, we check conditions in ascending order
    // of the expected computational overhead
    general_discard = t->r_includes.n_paths == 0 ||
                      (sets->r_only_writable && ! may_write);

    store_discard =  t->script_includes.n_paths == 0 ||
                     (sets->r_store_only_writable && ! may_write  ) ||
                     inode->i_size > sets->r_store_max_size ||
                     t->stored_files_count >= sets->r_store_max_count_of_files;
    if(general_discard && store_discard){
        // maybe_todo: put this early-discard code directly into the
        // __fput-handler, to avoid the ringbuffer altogether.
        // __dbg_print_event(t, close_ev, "early discard");
        return;
    }

    d_ent = consumer_cache_find(
                    t->event_consumer.r_cache,
                    path->mnt, READ_ONCE(path->dentry->d_parent),
                &cache_entry_existed);
    if(IS_ERR(d_ent)){
        return;
    }
    if(cache_entry_existed){
        t->_dircache_hits++;
        general_discard |= d_ent->flags & DIRCACHE_R_OFF;
        store_discard |= d_ent->flags & DIRCACHE_SCRIPT_OFF;
    } else {
        general_discard |=
            !kpathtree_is_subpath(&t->r_includes,d_ent->dirname,d_ent->dirname_len,true) ||
             kpathtree_is_subpath(&t->r_excludes,d_ent->dirname,d_ent->dirname_len,true) ||
                  (sets->r_exclude_hidden &&
                  (path_is_hidden = __path_is_hidden(d_ent->dirname,d_ent->dirname_len)));
        store_discard |=
            !kpathtree_is_subpath(&t->script_includes,d_ent->dirname,d_ent->dirname_len,true) ||
             kpathtree_is_subpath(&t->script_excludes,d_ent->dirname,d_ent->dirname_len,true);

        if(! store_discard && sets->r_store_exclude_hidden){
            // use hidden result from above, if possible
            store_discard = (path_is_hidden != -1)
                    ? path_is_hidden
                    : __path_is_hidden(d_ent->dirname,d_ent->dirname_len);
        }
        d_ent->flags = 0;
        if(general_discard){
            d_ent->flags |= DIRCACHE_R_OFF;
        }
        if(store_discard){
            d_ent->flags |= DIRCACHE_SCRIPT_OFF;
        }
    }
    if(general_discard && store_discard){
        return;
    }
    // file only settings

    kutil_take_name_snapshot(&name_snapshot, path->dentry);
    filename = &name_snapshot.name;

    general_discard |= (sets->r_exclude_hidden && filename->name[0] == '.');

    store_discard |= (sets->r_store_exclude_hidden && filename->name[0] == '.') ||
           (t->script_ext.n_ext &&
            ! file_extensions_contain(&t->script_ext, (const char*)filename->name,
                                      filename->len));

    if(general_discard && store_discard){
        // pr_devel("discarding %s\n", filename);
        goto out_release;
    }

    // Capture store-events regardless of r_max_event_count
    if(store_discard && t->r_event_count >= sets->r_max_event_count){
        t->r_dropped_count++;
        goto out_release;
    }

    // user really wants this event
    if(likely(__do_log_file_event(t, close_ev, O_RDONLY, filename, !store_discard,
                        d_ent, &t->event_consumer.r_last_written_path ))){
        t->r_event_count++;
    }

out_release:
    kutil_release_name_snapshot(&name_snapshot);
}

static void
__handle_write_event(struct event_target* t,
                    struct close_event* close_ev,
                    bool may_write){
    const struct shounalk_settings* sets = &t->settings;
    struct kutil_name_snapshot name_snapshot;
    const struct inode* inode = close_ev->path.dentry->d_inode;
    struct path* path = &close_ev->path;
    struct consumer_cache_entry* d_ent;
    bool cache_entry_existed;

    t->w_examined_count++;

    // __dbg_print_event(t, close_ev, "processing wevent");

    if(inode->i_nlink == 0){
        t->w_deleted_count++;
        pr_devel("ignore deleted file\n");
        return;
    }
    if(unlikely(! may_write)){
        pr_devel("ignore not writable file\n");
        return;
    }

    d_ent = consumer_cache_find(
                    t->event_consumer.w_cache,
                    path->mnt, READ_ONCE(path->dentry->d_parent),
                &cache_entry_existed);
    if(IS_ERR(d_ent)){
        return;
    }
    // Check if we have seen and accepted our d_parent-dir before.
    if(cache_entry_existed){
        t->_dircache_hits++;
        if(d_ent->flags & DIRCACHE_W_OFF){
            return;
        }
    } else {
        if(!kpathtree_is_subpath(&t->w_includes,d_ent->dirname,d_ent->dirname_len,true) ||
            kpathtree_is_subpath(&t->w_excludes,d_ent->dirname,d_ent->dirname_len,true) ||
            (sets->w_exclude_hidden && __path_is_hidden(d_ent->dirname,d_ent->dirname_len)))
        {
            d_ent->flags = DIRCACHE_W_OFF;
            return;
        }
        d_ent->flags = 0;
    }
    // directory was accepted - check file:
    kutil_take_name_snapshot(&name_snapshot, path->dentry);

    if(sets->w_exclude_hidden && name_snapshot.name.name[0] == '.'){
        goto out_release;
    }
    if(t->w_event_count >= sets->w_max_event_count){
        t->w_dropped_count++;
        goto out_release;
    }

    // user really wants this event
    if(likely(__do_log_file_event(t, close_ev, O_WRONLY, &name_snapshot.name, false,
                        d_ent, &t->event_consumer.w_last_written_path))){
        t->w_event_count++;
    }

out_release:
    kutil_release_name_snapshot(&name_snapshot);
}

////////////////////////////////////////////////////////////////////////////

long event_consumer_init(struct event_consumer* consumer){
    memset(consumer, 0, sizeof (struct event_consumer));

    // To avoid alignment of struct close_event to buffer size,
    // we simply allocate a little more space, so we do not
    // overflow right before the ring-buffer wrap-around.
    consumer->circ_buf.buf = kvzalloc(CONSUMER_CIRC_BUFSIZE + sizeof (struct close_event),
                                      SHOURNALK_GFP | __GFP_RETRY_MAYFAIL);
    if(! consumer->circ_buf.buf)
        return -ENOMEM;
    consumer->w_cache = kvzalloc(sizeof (struct consumer_cache),
                                 SHOURNALK_GFP | __GFP_RETRY_MAYFAIL);
    if(! consumer->w_cache)
        goto err1;

    consumer->r_cache = kvzalloc(sizeof (struct consumer_cache),
                                 SHOURNALK_GFP | __GFP_RETRY_MAYFAIL);
    if(! consumer->r_cache)
        goto err2;

    consumer->circ_buf_size = CONSUMER_CIRC_BUFSIZE;
    spin_lock_init(&consumer->queue_lock);
    sema_init(&consumer->start_sema, 0);

    consumer_cache_init(consumer->w_cache);
    consumer_cache_init(consumer->r_cache);


    return 0;

err2:
    kvfree(consumer->w_cache);
err1:
    kvfree(consumer->circ_buf.buf);
    return -ENOMEM;
}

void event_consumer_cleanup(struct event_consumer* c){
    if(! IS_ERR_OR_NULL(c->consume_task)){
        put_task_struct(c->consume_task);
    }

    kvfree(c->r_cache);
    kvfree(c->w_cache);
    kvfree(c->circ_buf.buf);
}


long event_consumer_thread_create(struct event_target* event_target,
                        const char* thread_name){
    struct event_consumer* consumer = &event_target->event_consumer;
    consumer->consume_task = kthread_create(event_queue_consume_thread,
                                            event_target,
                                            "%s", thread_name);

    if(IS_ERR(consumer->consume_task)){
        pr_warn("Failed to create consume thread %s - %ld\n", thread_name,
                PTR_ERR(consumer->consume_task));
        return PTR_ERR(consumer->consume_task);
    }
    get_task_struct(consumer->consume_task);

    wake_up_process(consumer->consume_task);

    // see documentation of kthread_stop (linux 4.19):
    // if kthread is stopped very early,
    // the threadfn might *never* be called. Here this might happen
    // during the observation of short-lived processes.
    // We must however make sure, it runs at least once, as
    // events might be pending. So wait, until our thread calls "up".
    down(&consumer->start_sema);

    return 0;
}

void event_consumer_thread_setup(struct event_target* event_target){
    struct event_consumer* consumer = &event_target->event_consumer;

    if(event_target->mm) {
#ifdef USE_MM_SET_FS_OFF
        consumer->consume_tsk_oldfs = get_fs();
        set_fs(USER_DS);
#endif
        kutil_use_mm(event_target->mm);
    }

    // use_mm() -> We want this kthread's page-cache memory allocations to account to
    // the callers memcg. At least on linux 4.19 and ext4 using the mm of the
    // caller should suffice. See also below stacktrace, which shows how
    // exactly the memcg is used. Note however that mm->owner might be set to null,
    // in case our parent process exits or execs, so we will only keep on logging file
    // events, if current->mm->owner == event_target->caller_tsk. See
    // also exit.c:mm_update_next_owner
    //
    // # ext4 mem_cgroup charging
    // First the mem_cgroup is associated with a page:
    // (some parts of the stacktrace were omitted for better readability).
    //
    // ext4_da_write_begin
    // grab_cache_page_write_begin
    //     pagecache_get_page
    //         __alloc_pages_nodemask
    //         add_to_page_cache_lru
    //         __add_to_page_cache_locked
    //             mem_cgroup_try_charge(current->mm) <-- !
    //                 get_mem_cgroup_from_mm         <-- owner != null for correct accounting
    //                 try_charge
    //                 memcgroup_commit_charge: page->mem_cgroup = memcg; <-- !
    //
    // Then the mem_cgroup is taken later
    //
    // ext4_block_write_begin
    //     create_empty_buffers
    //         alloc_page_buffers ( in fs/buffer.c, uses __GFP_ACCOUNT!)
    //             get_mem_cgroup_from_page: memcg = page->mem_cgroup;   <-- !
    //             alloc_buffer_head
    //                 kmem_cache_alloc


    // set_user_nice(current, 1); // 2? 10? MAX_NICE?
    // Only affects reads. See also: https://unix.stackexchange.com/a/480863/288001
    set_task_ioprio(current, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 6));

    // process events with user credentials
    consumer->consume_task_orig_cred = override_creds(event_target->cred);
}

void event_consumer_thread_cleanup(struct event_target* event_target){
    struct event_consumer* consumer = &event_target->event_consumer;

    kutil_WARN_ONCE_IFN_DBG(current != consumer->consume_task,
                            "current != consumer->consume_task");
    revert_creds(consumer->consume_task_orig_cred);
    if(event_target->mm){
        kutil_unuse_mm(event_target->mm);
#ifdef USE_MM_SET_FS_OFF
        set_fs(consumer->consume_tsk_oldfs);
#endif
    }
}

void event_consumer_thread_stop(struct event_consumer* consumer){
    int ret;
    if( (ret = kthread_stop(consumer->consume_task))){
        kutil_WARN_ONCE_IFN_DBG(1, "event-consume-thread returned %d\n", ret);
    }
}

bool event_consumer_flush_target_file_safe(struct event_target *t)
{
    ssize_t ret;
    if(unlikely((ret = shournal_kio_flush(t->file)) < 0)){
        WRITE_ONCE(t->ERROR, true);
        pr_debug("Failed to flush event target file %s, returned %ld\n",
                 t->file_init_path, ret);
        event_target_write_result_to_user_ONCE(t, (int)-ret);
        return false;
    }
    return true;
}


void close_event_consume(struct event_target* event_target, struct close_event* close_ev){
    bool may_read;
    bool may_write;

    if(unlikely(event_target->ERROR)){
        goto out;
    }
    if(unlikely(! close_ev->path.dentry->d_inode)){
        pr_devel("ignore event, inode is NULL.\n");
        goto out;
    }
    may_read =  kutil_inode_permission(&close_ev->path, MAY_READ) == 0;
    if(unlikely(! may_read)){
        // maybe the file event came from a setuid-program? Otherwise, the file
        // might be writable, but not readable. We ignore this special
        // case here, because we cannot (securely) hash it anyway.
        __dbg_print_event(event_target, close_ev, "may_read is false");
        goto out;
    }
    // __dbg_print_event(event_target, close_event, "test");
    may_write = kutil_inode_permission(&close_ev->path, MAY_WRITE) == 0;

    // Just as fanotify does, we consider O_RDWR only
    // as write-event.
    // maybe_todo: differentiate?
    if(close_ev->f_mode & FMODE_WRITE){
        __handle_write_event(event_target, close_ev, may_write);
    } else {
        __handle_read_event(event_target, close_ev, may_write);
    }

out:
    close_event_cleanup(close_ev);
}


void close_event_cleanup(struct close_event* event){
    dput(event->path.dentry);
    mntput(event->path.mnt);
}


