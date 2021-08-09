#pragma once

#include "shournalk_global.h"

#include <linux/cgroup.h>
#include <linux/dcache.h>
#include <linux/bug.h>
#include <linux/fs.h>
#include <linux/memcontrol.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/limits.h>
#include <asm/syscall.h>
#include <asm/uaccess.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
#define mmgrab _mmgrab_backport
static inline void _mmgrab_backport(struct mm_struct *mm) {
    atomic_inc(&mm->mm_count);
}
#else
#include <linux/sched/mm.h>
#endif

struct pipe_inode_info;

#ifdef DEBUG
#define kutil_WARN_DBG WARN
#define kutil_WARN_ON_DBG WARN_ON
// if DEBUG always warn, else only once
#define kutil_WARN_ONCE_IFN_DBG WARN
#else
#define kutil_WARN_DBG(condition, format...)
#define kutil_WARN_ON_DBG(condition)
#define kutil_WARN_ONCE_IFN_DBG WARN_ONCE
#endif

// see commit dcda9b04713c3f6ff0875652924844fae28286ea
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)) && \
     !defined __GFP_RETRY_MAYFAIL
#define __GFP_RETRY_MAYFAIL __GFP_REPEAT

#endif

// see commit a7c3e901a46ff54c016d040847eda598a9e3e653
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
#define KVMALLOC_BACKPORT

#define kvmalloc_node _kvmalloc_node_backport
void* _kvmalloc_node_backport(size_t size, gfp_t flags, int node);

#define kvmalloc _kvmalloc_backport
static inline void* _kvmalloc_backport(size_t size, gfp_t flags)
{
    return kvmalloc_node(size, flags, NUMA_NO_NODE);
}

#define kvzalloc _kvzalloc_backport
static inline void* _kvzalloc_backport(size_t size, gfp_t flags)
{
    return kvmalloc(size, flags | __GFP_ZERO);
}

#endif // KVMALLOC_BACKPORT


static inline int kutil_kthread_be_nice(void){
    return cond_resched();
}

char* resolve_reg_filepath(struct files_struct *files,
                           struct file *file,
                           char * buf);

ssize_t kutil_kernel_write(struct file *file, const void *buf, size_t count, loff_t *pos);
ssize_t kutil_kernel_write_locked(struct file * file, const void *buf, size_t count);


static inline ssize_t
kutil_kernel_read(struct file *file, void *buf, size_t count, loff_t *pos){
    ssize_t ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    ret = kernel_read(file, buf, count, pos);
#else
    mm_segment_t fs_save;

    fs_save = get_fs();
    set_fs(get_ds());
    ret = vfs_read(file, buf, count, pos);
    set_fs(fs_save);
#endif
    return ret;

}
ssize_t kutil_kernel_read_locked(struct file *file, void *buf, size_t count);
ssize_t
kutil_kernel_read_cachefriendly(struct file *file, void *buf, size_t count, loff_t *pos);

typedef unsigned long sysargs_t[6];
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0))
#define SYSCALL_GET_ARGUMENTS(current, regs, args) syscall_get_arguments((current), (regs), 0, 6, (args))
#else
#define SYSCALL_GET_ARGUMENTS(current, regs, args) syscall_get_arguments((current), (regs), (args))
#endif

static inline unsigned long SYSCALL_GET_FIRST_ARG(struct task_struct *task,
                                  struct pt_regs *regs){
    // Optimization for:
#ifdef CONFIG_X86_64
    unsigned long val;
#ifdef CONFIG_IA32_EMULATION
// see b9d989c7218ac922185d82ad46f3e58b27a4bea9
// and 37a8f7c38339b22b69876d6f5a0ab851565284e3
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 8, 0) && \
    LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0) && \
    !defined TIF_SPEC_FORCE_UPDATE
    if (task->thread.status & TS_COMPAT){
#else
    // if (task_thread_info(task)->status & TS_COMPAT){
    if (task->thread_info.status & TS_COMPAT) {
#endif
        val = regs->bx;
    } else
#endif
    {
        val = regs->di;
    }
    return val;
#else
    sysargs_t args;
    SYSCALL_GET_ARGUMENTS(task, regs, args);
    return args[0];
#endif
}


struct kutil_name_snapshot {
    struct qstr name;
    unsigned char inline_name[NAME_MAX + 1];
};

void kutil_take_name_snapshot(struct kutil_name_snapshot *, struct dentry *);
void kutil_release_name_snapshot(struct kutil_name_snapshot*);


// Replacement for the older memalloc_use_memcg, memalloc_unuse_memcg,
// see also commit b87d8cefe43c7f22e8aa13919c1dfa2b4b4b4e01
// Actually (I think) it should be possible to call
// set_active_memcg from KERNEL_VERSION(5, 10, 0) onwards
// _but_ int_active_memcg is not exported as of 5.14.
// current->active_memcg was introduced by
// d46eb14b735b11927d4bdc2d1854c311af19de6d
#if defined CONFIG_MEMCG && \
       (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
static inline struct mem_cgroup *
kutil_set_active_memcg(struct mem_cgroup *memcg)
{
    struct mem_cgroup *old;
    if (unlikely(in_interrupt())) {
        kutil_WARN_ONCE_IFN_DBG(1, "Called in_interrupt...");
        return NULL;
    }

    old = current->active_memcg;
    current->active_memcg = memcg;
    return old;
}
#else
static inline struct mem_cgroup *
kutil_set_active_memcg(struct mem_cgroup *memcg)
{
    (void)(memcg);
    return NULL;
}
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0))
// see commit 37c54f9bd48663f7657a9178fe08c47e4f5b537b
#define USE_MM_SET_FS_OFF
// see commit f5678e7f2ac31c270334b936352f0ef2fe7dd2b3
#define kthread_use_mm use_mm
#define kthread_unuse_mm unuse_mm
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)) && \
    !defined (INIT_RCU_WORK)
#define RCU_WORK_BACKPORT
#endif

#ifdef RCU_WORK_BACKPORT

struct rcu_work {
    struct work_struct work;
    struct rcu_head rcu;

    /* target workqueue ->rcu uses to queue ->work */
    struct workqueue_struct *wq;
};
static inline struct rcu_work *to_rcu_work(struct work_struct *work)
{
    return container_of(work, struct rcu_work, work);
}

#define INIT_RCU_WORK(_work, _func)					\
    INIT_WORK(&(_work)->work, (_func))

bool queue_rcu_work(struct workqueue_struct *wq, struct rcu_work *rwork);

#endif // RCU_WORK_BACKPORT


#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
#define GET_MEMCG_FROM_MM_BACKPORT

#define get_mem_cgroup_from_mm _get_mem_cgroup_from_mm_backport
struct mem_cgroup *_get_mem_cgroup_from_mm_backport(struct mm_struct *mm);

#define mem_cgroup_put _mem_cgroup_put_backport
static inline void _mem_cgroup_put_backport(struct mem_cgroup *memcg) {
    if (memcg)
        css_put(&memcg->css);
}
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
#define vfs_fadvise _vfs_fadvise_dummy
static inline int
_vfs_fadvise_dummy(struct file *file, loff_t offset, loff_t len,
                              int advice){
    (void)file;
    (void)(offset);
    (void)(len);
    (void)(advice);
    return 0;
}
#endif


// see commit 47291baa8ddfdae10663624ff0a15ab165952708
// and        a6435940b62f81a1718bf2bd46a051379fc89b9d
static inline int
kutil_inode_permission(struct user_namespace *user_ns, struct inode * inode, int mask){
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 12, 0)) && \
    !defined FS_ALLOW_IDMAP
    (void)(user_ns);
    return inode_permission(inode, mask);
#else
    return inode_permission(user_ns, inode, mask);
#endif

}


