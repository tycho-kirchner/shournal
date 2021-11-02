#pragma once

#include "shournalk_global.h"

#include <linux/types.h>

struct event_target;
struct ftrace_ops;
struct pt_regs;

int  event_handler_constructor(void);
void event_handler_destructor(void);

long event_handler_add_pid(struct event_target*, pid_t, bool collect_exitcode);
long event_handler_remove_pid(pid_t pid);

noinline notrace
void event_handler_fput(unsigned long, unsigned long,
                        struct ftrace_ops*, struct pt_regs*);

void event_handler_process_exit(struct task_struct *task);

void event_handler_process_fork(struct task_struct *parent,
                                  struct task_struct *child);

