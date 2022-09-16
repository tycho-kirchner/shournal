#pragma once

#include "shournalk_global.h"

#include <linux/path.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/limits.h>
#include <linux/timer.h>
#include <linux/compiler.h>
#include <linux/circ_buf.h>

#include "kutil.h"

struct event_target;
struct consumer_cache;

struct close_event {
    struct path path;
    fmode_t f_mode;
};

struct event_consumer {
    struct circ_buf circ_buf;
    struct spinlock queue_lock;
    int circ_buf_size;
    bool woken_up;
    struct task_struct* consume_task;

    const struct cred* consume_task_orig_cred;
    struct consumer_cache* w_cache;
    struct path w_last_written_path; /* last logged full path */
    struct consumer_cache* r_cache;
    struct path r_last_written_path;
    struct semaphore start_sema;
#ifdef USE_MM_SET_FS_OFF
       mm_segment_t consume_tsk_oldfs;
#endif

};


long event_consumer_init(struct event_consumer*);
void event_consumer_cleanup(struct event_consumer*);

long event_consumer_thread_create(struct event_target* event_target,
                        const char* thread_name);
void event_consumer_thread_setup(struct event_target* event_target);
void event_consumer_thread_cleanup(struct event_target* event_target);

void event_consumer_thread_stop(struct event_consumer* consumer);

bool event_consumer_flush_target_file_safe(struct event_target*);


void close_event_consume(struct event_target*, struct close_event*);
void close_event_cleanup(struct close_event* event);


