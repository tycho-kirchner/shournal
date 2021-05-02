
#include <linux/ftrace.h>
#include <linux/tracepoint.h>
#include <linux/version.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/task_work.h>


struct ftrace_ops;

#include "tracepoint_helper.h"
#include "event_handler.h"
#include "event_queue.h"
#include "event_consumer.h"
#include "kutil.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0))
#define USE_LEGACY_TRACEPOINTS
#endif

/// Type of kernel traces used in here.
enum {SHOURNALK_TP_TRACE, SHOURNALK_TP_FTRACE};


noinline notrace static void
__probe_sched_process_fork(void *data __attribute__ ((unused)),
                         struct task_struct *parent,
                         struct task_struct *child) {
    event_handler_process_fork(parent, child);
}

noinline notrace static void
__probe_process_exit(unsigned long ip __attribute__ ((unused)),
                     unsigned long parent_ip __attribute__ ((unused)),
                     struct ftrace_ops *op __attribute__ ((unused)),
                     struct pt_regs *regs){
    struct task_struct *task;
    task = (struct task_struct*)(SYSCALL_GET_FIRST_ARG(current, regs));
    event_handler_process_exit(task);
}

/// Common structure to hold ftraces and tracepoints.
struct trace_entry {
    char name[KSYM_NAME_LEN]; /* Don't use char* here! */
    void *func; /* our own probe */
    int tp_type; /* SHOURNALK_TP_TRACE, SHOURNALK_TP_FTRACE ... */
    unsigned long flags;
    void *tracepoint; /* tracepoint in kernel */
    bool init;
    struct ftrace_ops __ftrace_ops;
};

// TODO: handle patch from October 2020
// PATCH 9/9] ftrace: Reverse what the RECURSION flag means in the ftrace_ops
#define __DEFAULT_FTRACE_FLAGS \
            FTRACE_OPS_FL_SAVE_REGS | FTRACE_OPS_FL_RECURSION_SAFE  // FTRACE_OPS_FL_RCU


static struct trace_entry interests[] = {

    {.name = "sched_process_fork", .func = (void*)__probe_sched_process_fork,
     .tp_type = SHOURNALK_TP_TRACE },

    // Look at kernel/exit.c::do_exit : we need to run our exit
    // hook *after* the remaining open files were closed,
    // otherwise we would loose those events. Thus, the tracepoint
    // sched_process_exit is too early. sched_process_free on the other hand
    // runs too late, probably because free is called after parent processes
    // finished waiting but we want to allow waiting for them.
    // perf_event_exit_task would probably be ideal, but cannot be traced, but
    // cgroup_exit (or exit_notify) seems to be fine. Note that some of the
    // functions called within
    // do_exit are inlined (dependent on kernel version), thus cannot be ftraced.
    {.name = "cgroup_exit", .func = (void*)__probe_process_exit,
     .tp_type = SHOURNALK_TP_FTRACE,
     .flags = __DEFAULT_FTRACE_FLAGS},

    // cannot use inline function fsnotify_close.
    // __close_fd is too highlevel (doesn't trigger on process exit,
    // CLO_EXEC files and (probably dup(2), etc.).
    // By using locks_remove_file instead of __fput, we avoid duplicate check
    // #ifdef FMODE_OPENED
    //             unlikely(!(file->f_mode & FMODE_OPENED)) ||
    // #endif
    {.name = "locks_remove_file",  .func = (void*)event_handler_fput,
     .tp_type = SHOURNALK_TP_FTRACE,
     .flags = __DEFAULT_FTRACE_FLAGS},

};


static void init_ftrace_entry(struct trace_entry* e){
    e->__ftrace_ops.func = e->func;
    e->__ftrace_ops.flags =e->flags;
}


#define FOR_EACH_INTEREST(i) \
    for (i = 0; i < sizeof(interests) / sizeof(struct trace_entry); i++)

static void init_interests(void){
    size_t i;
    FOR_EACH_INTEREST(i) {
        interests[i].init = 0;
        if(interests[i].tp_type == SHOURNALK_TP_FTRACE){
            init_ftrace_entry(&interests[i]);
        }
    }
}


#ifndef USE_LEGACY_TRACEPOINTS
// Tracepoints are not exported. Look them up.
static void lookup_tracepoints(struct tracepoint *tp,
                               void *ignore __attribute__ ((unused)) ) {
    size_t i;
    FOR_EACH_INTEREST(i) {
        // pr_info("tracepoint: %s\n", tp->name);
        if (strcmp(interests[i].name, tp->name) == 0){
            interests[i].tracepoint = tp;
        }
    }
}
#endif // USE_LEGACY_TRACEPOINTS

static int __register_tracepoint(struct trace_entry * entry){
    int ret;
#ifndef USE_LEGACY_TRACEPOINTS
    if (entry->tracepoint == NULL) {
        return -ENXIO;
    }

    ret = tracepoint_probe_register(entry->tracepoint, entry->func, NULL);
#else
    ret = tracepoint_probe_register(entry->name, entry->func, NULL);
#endif
    entry->init = ret == 0;
    return ret;
}

static int __unregister_tracepoint(struct trace_entry * entry){
    int ret;
#ifndef USE_LEGACY_TRACEPOINTS
    ret = tracepoint_probe_unregister(entry->tracepoint, entry->func, NULL);
#else
    ret = tracepoint_probe_unregister(entry->name, entry->func, NULL);
#endif
    entry->init = 0;
    return ret;
}


static int __register_ftrace(struct trace_entry * entry){
    int ret;

    if((ret = ftrace_set_filter(&entry->__ftrace_ops, entry->name, strlen(entry->name), 0)) < 0){
        pr_warn("ftrace_set_filter %s failed\n", entry->name);
        return ret;
    }
    ret = register_ftrace_function(&entry->__ftrace_ops);
    entry->init = ret == 0;
    return ret;
}

static int __unregister_ftrace(struct trace_entry * entry){
    int ret;
    ret = unregister_ftrace_function(&entry->__ftrace_ops);
    entry->init = 0;
    return ret;
}



static void cleanup(void) {
    size_t i;
    FOR_EACH_INTEREST(i) {
        int ret = 0;
        struct trace_entry* e = &interests[i];
        if (! e->init) {
            continue;
        }

        switch (e->tp_type) {
        case SHOURNALK_TP_TRACE: ret = __unregister_tracepoint(e); break;
        case SHOURNALK_TP_FTRACE: ret = __unregister_ftrace(e); break;
        default: WARN_ON(1); break;
        }
        if(ret != 0){
            pr_warn("failed to unregister trace %s\n", e->name);
        }

    }
}



int tracepoint_helper_constructor(void) {
    size_t i;
    int ret = 0;
    init_interests();

#ifndef USE_LEGACY_TRACEPOINTS
    for_each_kernel_tracepoint(lookup_tracepoints, NULL);
#endif

    FOR_EACH_INTEREST(i) {
        struct trace_entry* e = &interests[i];
        switch (e->tp_type) {
        case SHOURNALK_TP_TRACE: ret = __register_tracepoint(e);break;
        case SHOURNALK_TP_FTRACE: ret =  __register_ftrace(e);break;
        default: WARN_ON(1); ret = -1; break;
        }
        if(ret != 0){
            pr_warn("Failed to register trace %s\n", e->name);
            // Unload previously loaded
            cleanup();
            return ret;
        }
    }
    return 0;
}


void tracepoint_helper_destructor(void) {
    cleanup();
}

