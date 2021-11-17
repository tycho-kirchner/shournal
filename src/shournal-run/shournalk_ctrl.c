#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>

#include "shournalk_ctrl.h"

#define SHOURNALK_CTRL_PATH "/sys/kernel/shournalk_root/shournalk_ctrl"

// Unprivileged docker containers have a read-only sysfs filesystem.
// So we look at below path where the hosts shournalk-control
// might be bind-mounted (by the user) inside the container.
#define SHOURNALK_CTRL_DOCKER_PATH "/tmp/shournalk-sysfs/shournalk_ctrl"


#define SHOURNALK_MARK_PATH SHOURNALK_CTRL_PATH "/mark"
#define SHOURNALK_DOCKER_MARK_PATH SHOURNALK_CTRL_DOCKER_PATH "/mark"

#define SHOURNALK_VERSION_PATH SHOURNALK_CTRL_PATH  "/version"

static bool __file_exists(const char* filename){
    struct stat buffer;
    return (stat (filename, &buffer) == 0);
}

static int __open_sysfs_mark(void){
    const int o_flags = O_WRONLY | O_CLOEXEC;

    int fd = open(SHOURNALK_MARK_PATH, o_flags);
    if(fd >= 0)
        return fd;

    if(errno != EROFS){
        perror("Failed to open shournalk's sysfs-interface at "
               SHOURNALK_MARK_PATH ". Is the kernel module loaded? modprobe shournalk");
        return -1;
    }

    // likely inside docker or another container - try alternative path
    fd = open(SHOURNALK_DOCKER_MARK_PATH, o_flags);
    if(fd >= 0)
        return fd;

    perror("Failed to open shournalk's sysfs-interface at\n"
           SHOURNALK_MARK_PATH " and\n" SHOURNALK_DOCKER_MARK_PATH
           ". Is the kernel module loaded? modprobe shournalk");
    return -1;
}


static int __shournalk_filter_common(struct shournalk_group* grp, unsigned int flags,
                                     int action)
{
    grp->__mark_struct.flags = flags;
    grp->__mark_struct.action = action;

    ssize_t writeRet = write(grp->__sysfs_mark_fd,
                             &grp->__mark_struct,
                             sizeof (grp->__mark_struct));
    if(writeRet != sizeof (grp->__mark_struct)){
        return errno;
    }
    return 0;
}

bool shournalk_module_is_loaded(void){
    return __file_exists(shournalk_versionpath());
}

const char* shournalk_versionpath(void){
    return SHOURNALK_VERSION_PATH;
}



/// @param flags: for creating the pipe, e.g. O_NONBLOCK
struct shournalk_group*
shournalk_init(unsigned int flags){
    struct shournalk_group* grp;
    int sysfs_fd;

    int pip_descr[2];
    if(pipe2(pip_descr, flags) == -1){
        perror("pipe");
        return NULL;
    }

    sysfs_fd = __open_sysfs_mark();
    if(sysfs_fd < 0){
        close(pip_descr[0]);
        close(pip_descr[1]);
        return NULL;
    }
    grp = (struct shournalk_group*)calloc(1, sizeof (struct shournalk_group));
    if(grp == NULL){
        close(pip_descr[0]);
        close(pip_descr[1]);
        return NULL;
    }
    grp->__mark_struct.target_fd = -1;
    grp->pipe_readend = pip_descr[0];
    grp->__mark_struct.pipe_fd = pip_descr[1];
    grp->__sysfs_mark_fd = sysfs_fd;
    return grp;
}

void shournalk_release(struct shournalk_group* grp){
    if(close(grp->pipe_readend) == -1){
        perror("shournalk_release close pipe readend");
    }
    if(close(grp->__sysfs_mark_fd) == -1){
         perror("shournalk_release sysfs_mark_fd");
    }
    if(grp->__mark_struct.pipe_fd != -1){
        if(close(grp->__mark_struct.pipe_fd)){
            perror("shournalk_release close pipe writeend");
        }
    }
    free(grp);
}

void shournalk_set_target_fd(struct shournalk_group* grp, int fd){
    grp->__mark_struct.target_fd = fd;
}


void shournalk_set_settings(struct shournalk_group* grp,
                            struct shounalk_settings* settings){
    grp->__mark_struct.settings = *settings;
}


/// Make sure to set target_fd and settings beforehand
int shournalk_filter_pid(struct shournalk_group* grp, unsigned int flags, pid_t pid)
{
    grp->__mark_struct.pid = pid;
    return __shournalk_filter_common(grp, flags, SHOURNALK_MARK_PID);
}

/// @param str_tpye: one of SHOURNALK_MARK_*
///                             R_INCL, R_EXCL
///                             W_INCL, W_EXCL
///                             SCRIPT_INCL, SCRIPT_EXCL, SCRIPT_EXTS
int shournalk_filter_string(struct shournalk_group* grp, unsigned int flags,
                          int str_tpye, const char* str){
    grp->__mark_struct.data = str;
    return __shournalk_filter_common(grp, flags, str_tpye);
}

int shournalk_commit(struct shournalk_group* grp){
    return __shournalk_filter_common(grp, SHOURNALK_MARK_COMMIT, 0);
}


/// cĺose the pipe write end to avoid deadlock in poll.
/// warning - may only be called once per shournalk-group.
/// After that you are not allowed to call other functions but
/// shournalk_release
int shournalk_prepare_poll_ONCE(struct shournalk_group* grp){
    assert(grp->__mark_struct.pipe_fd != -1);
    if(close(grp->__mark_struct.pipe_fd)){
        perror("shournalk_prepare_poll_ONCE close pipe writeend");
        return errno;
    }
    grp->__mark_struct.pipe_fd = -1;
    return 0;
}

/// Read the version-string from sysfs, returning 0 on success,
/// else nonzero with errno set.
int shournalk_read_version(struct shournalk_version* ver){
    int fd;
    ssize_t ret = 0;

    fd = open(SHOURNALK_VERSION_PATH, O_RDONLY);
    if(fd < 0)
        return fd;
    ret = read(fd, ver->ver_str, sizeof (ver->ver_str));
    if(ret < 0) goto out;
    if(ret == sizeof (ver->ver_str)){
        fprintf(stderr, "shournalk_read_version - "
                        "too large version string read (bug?)\n");
        errno = EFBIG;
        ret = -1;
        goto out;
    }
    ver->ver_str[ret] = '\0';
    ret = 0; // success

out:
    close(fd);
    return (int)ret;
}


