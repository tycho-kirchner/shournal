/* Common code for usage both in user and (linux-)kernelspace
 *
 */

#pragma once


#ifdef __KERNEL__
#include "shournalk_global.h"

#include <linux/types.h>
struct file;

#define XXH_COMMON_STATE struct xxh64_state
typedef struct file* xxh_common_file_t;

#else

#include <sys/types.h>
#include <stdint.h>
#include "xxhash.h"

#define XXH_COMMON_STATE XXH64_state_t
typedef int xxh_common_file_t;

#endif // __KERNEL__


#ifdef __cplusplus
extern "C" {
#endif

struct partial_xxhash {
    unsigned chunksize; /* read and hash that many bytes per chunk */
    int max_count_of_reads; /* do not read more than that many chunks */
    loff_t seekstep; /* determined by file size and max_count_of_reads */
    XXH_COMMON_STATE* xxh_state;
    char* buf;
    size_t bufsize;
};

struct partial_xxhash_result {
    uint64_t hash;
    unsigned long long count_of_bytes;   // number of read bytes
};


long partial_xxh_digest_file(xxh_common_file_t file,
                struct partial_xxhash* part_hash,
                struct partial_xxhash_result* result);


#ifdef __cplusplus
}
#endif
