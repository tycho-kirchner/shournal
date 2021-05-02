
#pragma once

#include "shournalk_global.h"

#include <linux/types.h>


struct file;

struct kbuffered_file {
    struct file* __file;
    char* __buf;
    ssize_t __pos;
    size_t __bufsize;
};


struct kbuffered_file* shournal_kio_from_file(struct file* file, size_t bufsize);
void shournal_kio_close(struct kbuffered_file* file);

ssize_t shournal_kio_write(struct kbuffered_file* file, const void *buf, size_t count);
ssize_t shournal_kio_flush(struct kbuffered_file* file);

