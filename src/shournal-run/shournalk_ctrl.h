/* Generic c-interface to control shournal's kernel module from
 * userspace. Besides the kernel module (header) it depends only on
 * system headers.
 */

#pragma once

#include <fcntl.h>

#include "shournalk_user.h"

#ifdef __cplusplus
extern "C" {
#endif


struct shournalk_group {
    int pipe_readend; /* kernel notifies us when done */
    int __sysfs_mark_fd;
    struct shournalk_mark_struct __mark_struct;
};

struct shournalk_version {
    char ver_str[256];
};

bool shournalk_module_is_loaded(void);
const char* shournalk_versionpath(void);


struct shournalk_group* shournalk_init(unsigned int flags);
void shournalk_release(struct shournalk_group* grp);


void shournalk_set_target_fd(struct shournalk_group* grp, int fd);
void shournalk_set_settings(struct shournalk_group* grp,
                            struct shounalk_settings* settings);


int shournalk_filter_pid(struct shournalk_group* grp, unsigned int flags, pid_t pid);
int shournalk_filter_string(struct shournalk_group* grp, unsigned int flags,
                           int str_tpye, const char* str);
int shournalk_commit(struct shournalk_group* grp);

int shournalk_prepare_poll_ONCE(struct shournalk_group* grp);

int shournalk_read_version(struct shournalk_version* ver);


#ifdef __cplusplus
}
#endif

