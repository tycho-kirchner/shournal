#pragma once
#include "shournalk_global.h"
#include <linux/version.h>
#include <linux/ftrace.h>

struct pt_regs;

int tracepoint_helper_constructor(void);

void tracepoint_helper_destructor(void);

static inline struct pt_regs*
tracepoint_helper_get_ftrace_regs(struct pt_regs* regs){
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)) && \
    ! defined arch_ftrace_get_regs
    return regs;
#else
    // see commit d19ad0775dcd64b49eecf4fa79c17959ebfbd26b and
    // 02a474ca266a47ea8f4d5a11f4ffa120f83730ad
    // ftrace: Have the callbacks receive a struct ftrace_regs
    // instead of pt_regs
    struct ftrace_regs* fregs = (struct ftrace_regs*)regs;
    return ftrace_get_regs(fregs);
#endif
}
