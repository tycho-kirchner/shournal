
#include "xxhash_common.h"

#ifdef __KERNEL__

#include <linux/errno.h>
#include <linux/fadvise.h>
#include <linux/fs.h>
#include <linux/types.h>


#include "kutil.h"
#include "xxhash_shournalk.h"

#else
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>

#define xxh64_update  XXH64_update
#define xxh64_reset   XXH64_reset
#define xxh64_digest  XXH64_digest
#define xxh64         XXH64
#endif

#ifndef min
#define min MIN
#endif

#include "user_kernerl.h"

static ssize_t __do_read(xxh_common_file_t file, void *buf, size_t nbytes){
#ifdef __KERNEL__
    return kutil_kernel_read_cachefriendly(file, buf, nbytes, &file->f_pos);
#else
    ssize_t ret = read(file, buf, nbytes);
    if(unlikely(ret < 0)){
        return -errno;
    }
    return ret;
#endif
}


static loff_t __do_seek(xxh_common_file_t file, loff_t offset){
#ifdef __KERNEL__
    file->f_pos += offset;
    return 0;
    // return vfs_llseek(file, offset, whence);
#else
    loff_t ret = lseek(file, offset, SEEK_CUR);
    if(unlikely(ret < 0)){
        return -errno;
    }
    return ret;   
#endif
}

/// read bufsize bytes from file and directly hash them
static ssize_t __read_and_hash(xxh_common_file_t file,
                                       void* buf, size_t bufsize,
                                       XXH_COMMON_STATE* xxh_state){
    ssize_t readBytes = __do_read(file, buf, bufsize);
    if(likely(readBytes > 0)){
        long xxh_ret = xxh64_update(xxh_state, buf, readBytes);
        // we always provide a valid buffer, so no
        // need to check for errors in production.
        kuassert(xxh_ret == 0);
        (void)xxh_ret; // avoid unused warning for release builds
    }
    return readBytes;
}


/// read a chunk of bytes from the file and hash it.
/// Do that multiple times, if the buffer-size is smaller
/// than the chunksize.
/// @return The number of read/hashed bytes or a neg. error.
static ssize_t __read_chunk(xxh_common_file_t file,
                                   struct partial_xxhash* part_hash){
    ssize_t readBytes;

    if(part_hash->chunksize <= part_hash->bufsize){
        readBytes = __read_and_hash(file, part_hash->buf, part_hash->chunksize,
                                    part_hash->xxh_state);
    } else {        
        // read and digest immediatly, as our buffer is small
        size_t missing_bytes = part_hash->chunksize;
        size_t readsize = part_hash->bufsize;
        while(true){
            ssize_t bytes = __read_and_hash(file, part_hash->buf, readsize,
                                            part_hash->xxh_state);
            if(unlikely(bytes < 0)) return bytes;

            missing_bytes -= bytes;
            kuassert(missing_bytes >= 0);
            if((size_t)bytes < readsize ||
                missing_bytes == 0){
                break;
            }
            if(missing_bytes < part_hash->bufsize){
                // almost done. Loop one last time with
                // a smaller read size
                readsize = missing_bytes;
            }
        }
        readBytes = part_hash->chunksize - missing_bytes;

    }
    return readBytes;
}



/// XXHASH-digest a whole file or parts of it at regular intervals.
/// @param file the fildescriptor of the file. Note that in general you would want
///           to make sure, that the offset is at 0. Note that the offset
///           may be changed during the call.
/// @param chunksize size of the chunks to read at once.
/// @param seekstep Read chunks from the file every seekstep bytes. The read chunk
///                 does not count into this, so if you actually want to skip bytes,
///                 seekstep must be greater than chunksize. Otherwise NO SEEK is
///                 performed at all.
/// @param maxCountOfReads stop reading and digest after that count of 'read'-
///                        operations.
/// @param result write hash and count of read bytes in here
/// @return 0 on success, else a positive error
long partial_xxh_digest_file(xxh_common_file_t file,
                struct partial_xxhash* part_hash,
                struct partial_xxhash_result* result
                )
{
    long err;
    int countOfReads;
    loff_t net_seek;
    result->count_of_bytes = 0;

    kuassert(part_hash->max_count_of_reads > 0);
    kuassert(part_hash->chunksize > 0);
    kuassert(part_hash->bufsize > 0);

    xxh64_reset(part_hash->xxh_state, 0);
    net_seek = part_hash->seekstep - part_hash->chunksize;

    for(countOfReads=0; countOfReads < part_hash->max_count_of_reads ; ++countOfReads) {
        // maybe_todo: preload next chunk with filemap.c:page_cache_read?
        ssize_t readBytes = __read_chunk(file, part_hash);
        if(unlikely(readBytes < 0)) return -readBytes;
        result->count_of_bytes += readBytes;
        if(readBytes < part_hash->chunksize) {
            break; // EOF
        }

        if( net_seek > 0  &&
                unlikely((err = __do_seek(file, net_seek)) < 0) ) {
            return -err;
        }
    }
    if(result->count_of_bytes == 0){
        result->hash = 0;
    } else {
        result->hash = xxh64_digest(part_hash->xxh_state);
    }
    return 0;
}
