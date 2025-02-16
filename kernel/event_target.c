

#include <linux/hashtable.h>
#include <linux/memory.h>
#include <linux/memcontrol.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/printk.h>
#include <linux/cred.h>
#include <linux/pipe_fs_i.h>
#include <linux/circ_buf.h>
#include <linux/user_namespace.h>

#include "event_target.h"
#include "kutil.h"
#include "shournal_kio.h"
#include "shournalk_user.h"
#include "xxhash_shournalk.h"


static struct file* __get_check_target_file(int fd){
    struct file* file;
    int error_nb = 0;

    file = fget(fd);
    if (!file){
        pr_devel("fget failed on target file\n");
        error_nb = -EBADF;
        goto err_cleanup_ret;
    }

    // We'll write to this file, so make sure we're allowed to
    if (!(file->f_mode & FMODE_WRITE)) {
        pr_debug("target file not writable\n");
        error_nb = -EPERM;
        goto err_cleanup_ret;
    }

    if (! S_ISREG(file_inode(file)->i_mode) ) {
        pr_debug("target file not a regular file\n");
        error_nb = -EBADFD;
        goto err_cleanup_ret;
    }
    return file;


err_cleanup_ret:
    if(! IS_ERR_OR_NULL(file)){
        fput(file);
    }
    return ERR_PTR(error_nb);
}


/// Get and reopen the pipe passed from userspace.
/// That way we are independent of the user-space
/// file status flags and always write with O_NONBLOCK
/// (a malicious user-space process might otherwise
/// block us indefinitely).
static struct file* __get_check_pipe(int pipe_fd){
    struct file* orig_pipe = NULL;
    struct file* new_pipe = NULL;
    long error_nb = 0;

    orig_pipe = fget(pipe_fd);
    if (!orig_pipe){
        pr_devel("fget failed on pipe-fd\n");
        error_nb = -EBADF;
        goto err_cleanup_ret;
    }
    if (! S_ISFIFO(file_inode(orig_pipe)->i_mode)) {
        pr_debug("passed fd not a FIFO\n");
        error_nb = -ENOTTY;
        goto err_cleanup_ret;
    }

    if (!(orig_pipe->f_mode & FMODE_WRITE)) {
        pr_debug("passed FIFO descriptor is not the write end\n");
        error_nb = -EPERM;
        goto err_cleanup_ret;
    }

    // With CONFIG_PROVE_LOCKING, kernel v5.10.191 a spurious "BUG: Invalid wait context"
    // occurred. Apparently, during dentry_open, a mutex is locked, thus previous code
    // calling dentry_open inside spin_lock(&current->files->file_lock) was buggy. Let's
    // remember this by calling:
    might_sleep();
    // reopen in nonblocking mode
    new_pipe = dentry_open(&orig_pipe->f_path,
                           O_WRONLY | O_NONBLOCK,
                           current->cred);
    if(!new_pipe){
        error_nb = -EXDEV;
        goto err_cleanup_ret;
    }
    if(IS_ERR(new_pipe)){
        error_nb = PTR_ERR(new_pipe);
        goto err_cleanup_ret;
    }
    fput(orig_pipe);

    return new_pipe;


err_cleanup_ret:
    if(! IS_ERR_OR_NULL(orig_pipe)) fput(orig_pipe);
    if(! IS_ERR_OR_NULL(new_pipe)) fput(new_pipe);

    return ERR_PTR(error_nb);
}



static struct event_target*
__event_target_create(struct file* target_file, struct file* pipe_w,
                     const struct shournalk_mark_struct * mark_struct){
    struct event_target* t = NULL;
    struct kbuffered_file* target_file_buffered = NULL;
    struct mem_cgroup* memcg = NULL;
    struct pid_namespace *pid_ns = task_active_pid_ns(current);
    char* path_tmp;
    long error = -ENOSYS;
    struct mm_struct * mm = NULL;

    if(!pid_ns){
        WARN(1, "pid_ns == NULL");
        return ERR_PTR(-ENXIO);
    }
    memcg = get_mem_cgroup_from_mm(current->mm);
    if(! memcg) {
        WARN(1, "memcg == NULL");
        return ERR_PTR(-ENXIO);
    }

    mm = get_task_mm(current);
    if(mm) {
        mmgrab(mm);
    } else {
        pr_debug("mm == NULL"); // does no real harm though
    }

    t = kvzalloc(sizeof (struct event_target), SHOURNALK_GFP | __GFP_RETRY_MAYFAIL);
    if(!t) {
        error = -ENOMEM;
        goto error_out;
    }

    t->partial_hash.bufsize = PAGE_SIZE;
    t->partial_hash.buf = kmalloc(t->partial_hash.bufsize, SHOURNALK_GFP);
    if(! t->partial_hash.buf){
        error = -ENOMEM;
        goto error_out;
    }

    t->partial_hash.xxh_state = kmalloc(sizeof (struct xxh64_state), SHOURNALK_GFP);
    if(!t->partial_hash.xxh_state){
        error = -ENOMEM;
        goto error_out;
    }

    path_tmp = d_path(&target_file->f_path,
                      t->file_init_path,
                      sizeof (t->file_init_path));
    if (IS_ERR(path_tmp)) {
        pr_debug("failed to resolve target file pathname\n");
        error = PTR_ERR(path_tmp);
        goto error_out;
    }
    memmove(t->file_init_path, path_tmp, strlen(path_tmp) + 1);

    target_file_buffered = shournal_kio_from_file(target_file, TARGET_FILE_BUFSIZE);
    if(IS_ERR(target_file_buffered)){
        error = PTR_ERR(target_file_buffered);
        goto error_out;
    }
    if((error = event_consumer_init(&t->event_consumer)) )
         goto error_out;

    t->exit_code = SHOURNALK_INVALID_EXIT_CODE;
    t->pid_ns = pid_ns;
    t->user_ns = current_user_ns();
    t->memcg = memcg;
    t->mm = mm;
    t->file = target_file_buffered;
    t->pipe_w = pipe_w;
    t->caller_tsk = current;
    atomic_set(&t->_written_to_user_pipe, 0);
    t->ERROR = false;

    kuref_set(&t->_f_count, 1);
    t->cred = current_cred();
    t->lost_event_count = 0;
    t->stored_files_count = 0;

    t->partial_hash.chunksize = mark_struct->settings.hash_chunksize;
    t->partial_hash.max_count_of_reads = mark_struct->settings.hash_max_count_reads;

    mutex_init(&t->lock);
    t->settings = mark_struct->settings;
    kpathtree_init(&t->w_includes);
    kpathtree_init(&t->w_excludes);
    kpathtree_init(&t->r_includes);
    kpathtree_init(&t->r_excludes);
    kpathtree_init(&t->script_includes);
    kpathtree_init(&t->script_excludes);

    file_extensions_init(&t->script_ext);


    // do not fail from here on, otherwise references would need
    // to be dropped again
    get_pid_ns(pid_ns);
    get_cred(current_cred());
    get_task_struct(current);
    get_user_ns(t->user_ns);


    return t;


error_out:
    if(! IS_ERR_OR_NULL(target_file_buffered)) kfree(target_file_buffered);
    if(! IS_ERR_OR_NULL(t)) {
        kfree(t->partial_hash.xxh_state);
        kvfree(t->partial_hash.buf);
        kvfree(t);
    }
    if(memcg) mem_cgroup_put(memcg);
    if(mm) { mmdrop(mm); mmput(mm); }

    return ERR_PTR(error);
}

static void __event_target_free(struct event_target* t){
    event_consumer_cleanup(&t->event_consumer);
    file_extensions_cleanup(&t->script_ext);
    kpathtree_cleanup(&t->w_includes);
    kpathtree_cleanup(&t->w_excludes);
    kpathtree_cleanup(&t->r_includes);
    kpathtree_cleanup(&t->r_excludes);
    kpathtree_cleanup(&t->script_includes);
    kpathtree_cleanup(&t->script_excludes);

    put_user_ns(t->user_ns);
    put_pid_ns(t->pid_ns);
    mem_cgroup_put(t->memcg);
    if(t->mm){ mmdrop(t->mm); mmput(t->mm); }
    put_task_struct(t->caller_tsk);

    put_cred(t->cred);
    shournal_kio_close(t->file);
    fput(t->pipe_w);
    kvfree(t->partial_hash.buf);
    kfree(t->partial_hash.xxh_state);
    kvfree(t);
}

static void __envent_target_destroy_work(struct work_struct *work){
    struct event_target* t = container_of(to_rcu_work(work),
                    struct event_target, destroy_rwork);
    __event_target_free(t);
}



////////////////////////////// public ////////////////////////////////////


// TODO: limit listeners per user to 128, like fanotify does.
/// event_target's are found by the struct file
/// where metadata about the file events are written to.
/// That way, multiple pid's can be marked for observation.
/// If no entry for the given target_fd exists, create a new one
/// (and store it in the hash-table). Once all processes finished
/// (or were unmarked again), notify userspace by writing into
/// the passed pipe.
struct event_target*
event_target_create(const struct shournalk_mark_struct * mark_struct)
{
    struct event_target* event_target = NULL;
    struct file* target_file = NULL;
    struct file* pipe_w = NULL;
    long error = -ENOSYS;

    target_file = __get_check_target_file(mark_struct->target_fd);
    if(IS_ERR(target_file)){
        return (void*)target_file;
    }

    pipe_w = __get_check_pipe(mark_struct->pipe_fd);
    if(IS_ERR(pipe_w)){
        error = PTR_ERR(pipe_w);
        goto err_put_unlock;
    }
    event_target = __event_target_create(target_file, pipe_w, mark_struct);
    if(IS_ERR(event_target) ){
        error = PTR_ERR(event_target);
        goto err_put_unlock;
    }

    // maybe_todo: add caller-pid to threadname?
    if((error = event_consumer_thread_create(event_target, "shournalk_consumer"))){
        goto err_put_unlock;
    }

    return event_target;

err_put_unlock:
    if(! IS_ERR_OR_NULL(event_target)){
        // ownership of pipe and target_file already transferred
        __event_target_free(event_target);
    } else {
        if(! IS_ERR_OR_NULL(pipe_w)) fput(pipe_w);
        if(! IS_ERR_OR_NULL(target_file)) fput(target_file);
    }
    return ERR_PTR(error);
}


// no events are registered before target is commited
long event_target_commit(struct event_target* t){
    WARN(! mutex_is_locked(&t->lock), "commit called without target lock\n");
    barrier();

    if( unlikely(event_target_is_commited(t))){
        pr_debug("event target already commited");
        return -EBUSY;
    }
    if(t->w_includes.n_paths){
        WRITE_ONCE(t->w_enable, true);
    }
    if(t->r_includes.n_paths || t->script_includes.n_paths){
        WRITE_ONCE(t->r_enable, true);
    }
    if( unlikely( ! event_target_is_commited(t))){
        // nothing marked - user did not specify any include-paths
        pr_debug("cannot commit - no include-paths registered");
        return -ENOTDIR;
    }
    return 0;
}

bool event_target_is_commited(const struct event_target* t){
    return READ_ONCE(t->w_enable) || READ_ONCE(t->r_enable);
}


/// Final put
void __event_target_put(struct event_target* event_target){
    long user_ret;
    int pending_bytes;
    struct event_consumer* consumer = &event_target->event_consumer;
    struct circ_buf* circ_buf = &consumer->circ_buf;
    bool we_are_consume_thread;

    might_sleep();

#ifdef DEBUG
    if(event_target->__dbg_flags){
        pr_info("event_target has dbg-flags set!\n");
        dump_stack();
    }
#endif
    we_are_consume_thread = current == consumer->consume_task;
    if(we_are_consume_thread){
        event_consumer_thread_cleanup(event_target);
    } else {
        // stopping the consumer thread also flushes the event buffer
        // one last time
        event_consumer_thread_stop(consumer);
    }

    pr_devel("Event processing done. Caller pid: %d - init target file path %s\n",
             event_target->caller_tsk->pid, event_target->file_init_path);
    pr_devel("consumed count: %lld, examined files: w: %lld, r: %lld\n",
            event_target->consumed_event_count,
            event_target->w_examined_count,
            event_target->r_examined_count);

    user_ret = shournal_kio_flush(event_target->file);
    if(user_ret >= 0){
        user_ret = 0;
    } else {
        pr_debug("final target-file flush failed with %ld\n", user_ret);
        user_ret = -user_ret;
    }
    pending_bytes = CIRC_CNT(READ_ONCE(circ_buf->head),
                             READ_ONCE(circ_buf->tail),
                             event_target->event_consumer.circ_buf_size);
    kutil_WARN_ONCE_IFN_DBG(pending_bytes != 0,
                            "pending bytes not 0 but %d", pending_bytes);

    event_target_write_result_to_user_ONCE(event_target, user_ret);

    // pr_info("dircache-hits: %lld, pathwrite_hits: %lld\n", event_target->_dircache_hits,
    //             event_target->_pathwrite_hits);

    if(current_work() == NULL){
        INIT_RCU_WORK(&event_target->destroy_rwork, __envent_target_destroy_work);
        // system_long_wq is flushed in event_handler_destructor, so do not change!
        queue_rcu_work(system_long_wq, &event_target->destroy_rwork);
    } else {
        __event_target_free(event_target);
    }

    if(we_are_consume_thread){
        // we just released the final reference - that's it.
        kutil_kthread_exit(NULL, 0);
    }
}




void event_target_write_result_to_user_ONCE(struct event_target* event_target, long error_nb){
    loff_t pos;
    ssize_t write_ret;
    struct shournalk_run_result result = {
        .error_nb = (int)error_nb,
        .w_event_count = event_target->w_event_count,
        .r_event_count = event_target->r_event_count,
        .lost_event_count = event_target->lost_event_count,
        .stored_event_count = event_target->stored_files_count,
        .selected_exitcode = event_target->exit_code
    };
    if(atomic_xchg(&event_target->_written_to_user_pipe, 1)){
        pr_devel("already written result (probably a previous error occurred");
        return;
    }
    pos = 0;
    write_ret = kutil_kernel_write(
                event_target->pipe_w, &result, sizeof(result), &pos);
    if(write_ret != sizeof(result)){
        pr_debug("Failed to write to user pipe - returned: %ld", write_ret);
    }
}

