/* Common helpers which can be used
 * from within user- and linux-kernel-
 * space
 */

#pragma once



#ifdef __KERNEL__

#ifdef DEBUG
#include <linux/bug.h>
// #define kuassert WARN_ON
#define kuassert(condition) WARN_ON(!(condition))

#else
#define kuassert(condition)
#endif

#else

#include <assert.h>
#define kuassert assert

#ifndef likely
#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif
#endif

#endif
