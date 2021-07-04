
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/delay.h>
#include <linux/thread_info.h>
#include <linux/interrupt.h>


#include "event_handler.h"
#include "event_target.h"
#include "event_queue.h"
#include "kutil.h"
#include "tracepoint_helper.h"


struct task_entry {
     struct task_struct* tsk;
     struct event_target* event_target; // here file events are written into
     struct hlist_node node ;
     struct rcu_work destroy_rwork;
} ;
DEFINE_HASHTABLE(static task_table, 16);
static struct kmem_cache * __task_entry_cache;

static DEFINE_SPINLOCK(task_table_lock);
static struct workqueue_struct* del_taskentries_wq = NULL;

static struct task_entry* __task_entry_alloc(void){
    return kmem_cache_alloc(__task_entry_cache,
                            GFP_NOWAIT | __GFP_ACCOUNT | __GFP_NOWARN);
}

/// Warning: May sleep!
static void
__task_entry_destroy(struct task_entry* e){
    event_target_put(e->event_target);
    kmem_cache_free(__task_entry_cache ,e);
}

static void __task_entry_destroy_work(struct work_struct *work){
    struct task_entry* el = container_of(to_rcu_work(work),
                    struct task_entry, destroy_rwork);
    __task_entry_destroy(el);
}


static inline u32
__task_hash(struct task_struct* task) {
    return (u32)(long)task;
}


static inline struct task_entry*
__find_task_entry(struct task_struct* task,
                            u32 task_hash){
    struct task_entry* el;
    hash_for_each_possible_rcu(task_table, el, node, task_hash) {
        if(el->tsk == task){
            return el;
        }
    }
    return NULL;
}


/// find and get a reference from task table under rcu_lock
static inline struct event_target*
__find_get_event_target_safe(struct task_struct* task){
    struct task_entry* el;
    struct event_target* event_target;
    u32 t_hash;
    t_hash = __task_hash(task);
    rcu_read_lock();   
    if((el = __find_task_entry(task, t_hash)) == NULL){
        rcu_read_unlock();
        return NULL;
    }

    event_target = event_target_get(el->event_target);
    rcu_read_unlock();
    return event_target;
}


/// Insert the given task into the table. If the task exists
/// and param update_if_exist is true, the event_target is updated, else
/// -EEXIST is returned. On success, the target's ref-counter is incremeneted.
static long
__insert_task_into_table_safe(struct task_struct *task,
                              struct event_target* target,
                              bool update_if_exist){
    struct task_entry* el;
    u32 t_hash;
    long ret = 0;
    struct event_target* old_target;
    t_hash = __task_hash(task);

    rcu_read_lock();
    // create-if-not-exist in same lock!
    spin_lock(&task_table_lock);
    if( likely((el=__find_task_entry(task, t_hash)) == NULL)) {
        // Whoever is interested in the events, pays for the allocation.
        struct mem_cgroup * oldcg;
        oldcg = kutil_set_active_memcg(target->memcg);
        el = __task_entry_alloc();
        kutil_set_active_memcg(oldcg);
        if(! el){
            ret = -ENOMEM;
            goto out_unlock;
        }
        el->tsk = task;
        el->event_target = event_target_get(target);
        hash_add_rcu(task_table, &el->node, t_hash);

        goto out_unlock;
    }
    // target exists. Fail, if update not allowed
    if(! update_if_exist){
        ret = -EEXIST;
        goto out_unlock;
    }
    old_target = el->event_target;
    // if old_target == new_target, nothing special happens,
    // as we first increment ref-counter.
    el->event_target = event_target_get(target);
    spin_unlock(&task_table_lock);
    rcu_read_unlock();

    // might_sleep, so put outside of lock
    event_target_put(old_target);

    return ret;

out_unlock:
    spin_unlock(&task_table_lock);
    rcu_read_unlock();
    return ret;
}


static bool
__remove_task_from_table_safe(struct task_struct *task){
    struct task_entry* el;
    bool enqueued = false;
    u32 t_hash;

    t_hash = __task_hash(task);
    rcu_read_lock();
    if((el = __find_task_entry(task, t_hash)) != NULL){
        spin_lock(&task_table_lock);
        hash_del_rcu(&el->node);
        spin_unlock(&task_table_lock);

        // free later
        INIT_RCU_WORK(&el->destroy_rwork, __task_entry_destroy_work);
        queue_rcu_work(del_taskentries_wq, &el->destroy_rwork);
        enqueued = true; // do not "enqueued = queue_rcu_work()", we do not care, if
                         // done now or later.
        // pr_devel("stop observing pid %d, init event path %s, caller %d\n",
        //           task->pid, el->event_target->file_init_path,
        //           el->event_target->caller_pid);
    }
    rcu_read_unlock();

    return enqueued;
}


static inline bool __fput_is_interesting(const struct file* file,
                                         const struct event_target* t){
    bool w_enable;
    bool r_enable;

    w_enable = READ_ONCE(t->w_enable);
    r_enable = READ_ONCE(t->r_enable);

    return (w_enable && file->f_mode & FMODE_WRITE) ||
           (r_enable && file->f_mode & FMODE_READ);
}



/// Check if @param task has same owner as current process
/// stolen from kernel/sched/core.c
static bool __task_check_same_owner(struct task_struct *task)
{
    const struct cred *cred_me = current_cred();
    const struct cred *cred_task;
    bool match;

    rcu_read_lock();
    cred_task = __task_cred(task);
    match = (uid_eq(cred_me->euid, cred_task->euid) ||
         uid_eq(cred_me->euid, cred_task->uid));
    rcu_read_unlock();
    return match;
}

static struct task_struct* __get_task_if_allowed(pid_t pid){
    struct task_struct* tsk;

    rcu_read_lock();
    tsk = get_pid_task(find_vpid(pid), PIDTYPE_PID);
    rcu_read_unlock();

    if( IS_ERR_OR_NULL(tsk)){
        // no such process in current pid-namespace
        pr_devel("pid %d does not exist in current pid namespace", pid);
        return ERR_PTR(-ESRCH);
    }
    if(! __task_check_same_owner(tsk)){
        pr_devel("pid %d has different owner", tsk->pid);
        put_task_struct(tsk);
        return ERR_PTR(-EPERM);
    }
    return tsk;
}


int event_handler_constructor(void) {    
    del_taskentries_wq = system_long_wq;
    hash_init(task_table);

    __task_entry_cache = KMEM_CACHE(task_entry, 0);
    if(! __task_entry_cache)
        return -ENOMEM;



    return 0;
}


void event_handler_destructor(void)
{
    u32 bucket;
    struct task_entry* el;
    struct hlist_node *temp_node;

    // First wait for all call_rcu() (called in queue_rcu_work) to
    // complete using rcu_barrier(), then flush the used workqueue.
    // Note that we are the only thread with access to the task_table,
    // since sysfs and tracepoints were already disabled. When this
    // function returns, all event_targets should have been freed.
    // See also Documentation/RCU/rcubarrier.txt: synchronize_rcu() is
    // *not* sufficent! We have to wait "for all outstanding RCU
    // callbacks to complete".
    rcu_barrier();
    flush_workqueue(del_taskentries_wq);

    hash_for_each_safe(task_table, bucket, temp_node, el, node) {
        hash_del(&el->node);
        __task_entry_destroy(el);
    }
    kmem_cache_destroy(__task_entry_cache);
}

/// Register param event_target as target for file events for the
/// given pid. The respective task must be running and
/// the caller must have the necessary capabilities. If the process is
/// already observed by another event_target we silently replace it
/// with the new one.
long event_handler_add_pid(struct event_target* event_target, pid_t pid){
    struct task_struct* task;
    long ret = 0;

    task = __get_task_if_allowed(pid);
    if(IS_ERR(task)){
        return PTR_ERR(task);
    }
    ret = __insert_task_into_table_safe(task, event_target, true);

    put_task_struct(task);
    return ret;
}

long event_handler_remove_pid(pid_t pid){
    struct task_struct* task;
    long ret = 0;

    task = __get_task_if_allowed(pid);
    if(IS_ERR(task)){
        return PTR_ERR(task);
    }
    if(! __remove_task_from_table_safe(task)){
        ret = -ESRCH;
    }
    put_task_struct(task);
    return ret;
}


/// If the current task shall be observed,
/// enqueue the file event for later processing.
/// Endless recursion is avoided by
/// using the flag FMODE_NONOTIFY and by observing
/// only regular files (so the target pipe does no harm
/// as well).
void event_handler_fput(unsigned long ip __attribute__ ((unused)),
                        unsigned long parent_ip __attribute__ ((unused)),
                        struct ftrace_ops *op __attribute__ ((unused)),
                        struct pt_regs *regs)
{
    struct event_target* event_target;
    struct file* file;

    file = (struct file*)(SYSCALL_GET_FIRST_ARG(current,
                                                tracepoint_helper_get_ftrace_regs(regs)));

    // Ideally we would ftrace fsnotify_close which is, however, inlined
    // (thus cannot be traced).
    // Below code is partially duplicated from there.
    if (file->f_mode & FMODE_NONOTIFY ||
        // maybe_todo: check file_inode(file) == NULL ifndef FMODE_OPENED
        !S_ISREG(file_inode(file)->i_mode)
        || unlikely(file->f_flags & FMODE_EXEC)
                     )
        return;

    // ftrace doc recommends to check this, however, __fput() calls dput() which
    // does rcu_read_lock() itself, so we should be safe.
    // if(! rcu_is_watching()) return;

    kutil_WARN_DBG(atomic_read(&file_inode(file)->i_count) < 1,
                   "file_inode(file)->i_count < 1");

    if((event_target = __find_get_event_target_safe(current)) == NULL ){
        return;
    }
    if( unlikely(! __fput_is_interesting(file, event_target)))
        goto out_put;

    // event_target ownership transferred to queue!
    event_queue_add(event_target, file);

    return;

out_put:
    // Might sleep!
    event_target_put(event_target);
}


void event_handler_process_exit(struct task_struct *task)
{
    if (unlikely(!rcu_is_watching())){
        kutil_WARN_DBG(1, "called without rcu");
        return;
    }
    __remove_task_from_table_safe(task);
}


/// If the parent task is observed and the child task is in
/// the same pid namespace, also add the child task to our
/// task_table
void event_handler_process_fork(struct task_struct *parent,
                                  struct task_struct *child){
    struct event_target* target;
    long ret;

    // fixme: is this ever called, if parent/child is kthread? Better check it.

    if (unlikely(!rcu_is_watching())){
        kutil_WARN_DBG(1, "called without rcu");
        return;
    }

    if((target = __find_get_event_target_safe(parent)) == NULL ){
        // parent not observed -> ignore child too
        return;
    }
    if(unlikely(READ_ONCE(target->ERROR))){
        pr_devel("Ignore fork of pid %d. Error flag "
                 "of event target is set.", parent->pid);
        goto put_out;
    }
    if(unlikely(task_active_pid_ns(child) != target->pid_ns)){
        pr_devel("pid namespace does not match "
                 "to parent with pid %d. Ignore.\n", parent->pid);
        goto put_out;
    }

    if(unlikely((ret = __insert_task_into_table_safe(child, target, false)))){
        pr_debug("failed to observe child process: %ld", ret);
        goto put_out;
    }

put_out:
    event_target_put(target);
}



