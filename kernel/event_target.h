
#pragma once

#include "shournalk_global.h"
#include "xxhash_common.h"

#include <linux/atomic.h>
#include <linux/limits.h>

#include "kpathtree.h"
#include "kfileextensions.h"
#include "shournalk_user.h"
#include "event_consumer.h"
#include "kutil.h"

// somewhat arbitrary, maybe raise?
#define PART_HASH_MAX_CHUNKSIZE 4096*16

#define TARGET_FILE_BUFSIZE  (1 << 15)

struct cred;
struct file;
struct kbuffered_file;
struct pid_namespace;
struct user_namespace;
struct shournalk_mark_struct;
struct dentry;


struct event_target {
    kuref_t _f_count; /* refcount - do not edit */

    bool w_enable; /* record write events */
    bool r_enable; /* record read events */
    bool ERROR; /* lazy-release references in case of an error */
    uint64_t lost_event_count;
    struct task_struct* exit_tsk; /* task for which to collect the exit code */
    int exit_code; /* see exit_tsk */

    struct event_consumer event_consumer;
    uint64_t consumed_event_count;
    struct file* pipe_w; /* write end of pipe. Id and bridge to user space group */
    struct kbuffered_file* file; /* write events in here from kernel space */
    const struct cred *cred; /* of the owner of the event target */
    uint64_t w_event_count; /* # logged write events */
    uint64_t w_dropped_count;  /* # dropped exceeding max_event_count */
    uint64_t w_deleted_count;  /* # file was deleted */
    uint64_t w_examined_count;  /* events taken a closer look at */
    uint64_t r_event_count; /* # logged read events */
    uint64_t r_dropped_count;  /* # dropped exceeding max_event_count */
    uint64_t r_deleted_count;  /* # file was deleted */
    uint64_t r_examined_count;  /* events taken a closer look at */

    unsigned stored_files_count;
    struct pid_namespace *pid_ns; /* we only follow forks in same pid ns */
    struct user_namespace* user_ns; /* of caller */
    struct mem_cgroup* memcg;     /* of caller */
    struct mm_struct* mm;         /* of caller */

    struct task_struct* caller_tsk; /* the caller interested in events. */

    struct shounalk_settings settings;
    struct partial_xxhash partial_hash;

    struct mutex lock; /* protects adding paths before committed */

    atomic_t _written_to_user_pipe; /* we write to user pipe only once */
    uint64_t _dircache_hits;
    uint64_t _pathwrite_hits;

    struct file_extensions script_ext;
    struct kpathtree w_includes;
    struct kpathtree w_excludes;
    struct kpathtree r_includes;
    struct kpathtree r_excludes;
    struct kpathtree script_includes;
    struct kpathtree script_excludes;

    char file_init_path[PATH_MAX];

    struct rcu_work destroy_rwork;
    int __dbg_flags;
};

struct event_target* event_target_create(const struct shournalk_mark_struct*);
long event_target_commit(struct event_target*);
bool event_target_is_commited(const struct event_target*);


static inline __attribute__((__warn_unused_result__))
struct event_target*
event_target_get(struct event_target* event){
    if(likely(kuref_inc_not_zero(&event->_f_count))){
        return event;
    }
    return NULL;
}

void __event_target_put(struct event_target* event_target);

static inline void
event_target_put(struct event_target* event_target){
#ifdef DEBUG
    might_sleep();
#endif

    if(unlikely( kuref_dec_and_test(&event_target->_f_count) )){
        __event_target_put(event_target);
    }
}


void event_target_write_result_to_user_ONCE(struct event_target*, long error_nb);

