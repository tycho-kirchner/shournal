
#pragma once
// include module name in print_* messages:
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "shournalk %s(): " fmt, __func__

#include <linux/gfp.h>

#define SHOURNALK_GFP GFP_KERNEL_ACCOUNT | __GFP_NOWARN

extern struct kpathtree g_dummy_pathtree;

long shournalk_global_constructor(void);
void shournalk_global_destructor(void);
