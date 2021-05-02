
#include "shournal_kio.h"
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/file.h>

#include "kutil.h"


struct kbuffered_file* shournal_kio_from_file(struct file* file, size_t bufsize){
    char* buf;
    struct kbuffered_file *buf_file;

    buf = kvmalloc(bufsize, SHOURNALK_GFP | __GFP_RETRY_MAYFAIL);
    if(! buf){
        return ERR_PTR(-ENOMEM);
    }
    buf_file = kmalloc(sizeof(struct kbuffered_file), SHOURNALK_GFP);
    if(buf_file == NULL){
        kvfree(buf);
        return ERR_PTR(-ENOMEM);
    }
    buf_file->__file = file;
    buf_file->__buf = buf;
    buf_file->__pos = 0;
    buf_file->__bufsize = bufsize;

    return buf_file;
}

void shournal_kio_close(struct kbuffered_file* file){
    shournal_kio_flush(file);
    fput(file->__file);
    kvfree(file->__buf);
    kfree(file);
}


ssize_t shournal_kio_write(struct kbuffered_file* file, const void *buf, size_t count){
    ssize_t ret;
    if(count > file->__bufsize){
        // flush and write as a whole
        if((ret = shournal_kio_flush(file)) < 0){
            return ret;
        }
        return kutil_kernel_write_locked(file->__file, buf, count);
    }

    if(file->__pos + count > file->__bufsize){
        if((ret = shournal_kio_flush(file)) < 0){
            return ret;
        }
    }
    memcpy(&file->__buf[file->__pos] , buf, count);
    file->__pos += count;
    return count;
}

/// @return The number of bytes written or neg. errno
ssize_t shournal_kio_flush(struct kbuffered_file* file){
    ssize_t ret;
    if(file->__pos == 0){
        return 0;
    }

    ret = kutil_kernel_write_locked(
                file->__file, file->__buf, file->__pos);
    if(ret < 0){
        return ret;
    }
    if(ret != file->__pos){
        // maybe_todo: mmove the rest of our buffer to the beginning?
        pr_devel("expected %ld bytes but wrote only %ld\n", file->__pos, ret);
        return -EIO;
    }
    file->__pos = 0;
    return ret;
}

