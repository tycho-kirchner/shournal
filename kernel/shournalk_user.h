/* Common header for kernel and userspace to control
 * shournalk via a sysfs interface
 */

#pragma once

#ifdef __KERNEL__

#include <linux/types.h>

#else

#include <sys/types.h>
#include <stdint.h>

#endif // __KERNEL__



#ifdef __cplusplus
extern "C" {
#endif

#define SHOURNALK_INVALID_EXIT_CODE    -1

/* flags */
#define SHOURNALK_MARK_ADD              0x00000001
#define SHOURNALK_MARK_REMOVE           0x00000002
/* start collecting events after commit */
#define SHOURNALK_MARK_COMMIT           0x00000004

/* If this flag is set on MARK_PID, on process-tree end, return the
   exitcode of the given pid in the run_result
   (selected_exitcode). If the process did not end
   (e.g. marked by another target) it is
   SHOURNALK_INVALID_EXIT_CODE */
#define SHOURNALK_MARK_COLLECT_EXITCODE 0x00000008


/* actions */
#define SHOURNALK_MARK_PID          100

#define SHOURNALK_MARK_SCRIPT_INCL  111 /* include paths */
#define SHOURNALK_MARK_SCRIPT_EXCL  112 /* exclude paths */
#define SHOURNALK_MARK_SCRIPT_EXTS  113 /* file extensions */

#define SHOURNALK_MARK_R_INCL       120
#define SHOURNALK_MARK_R_EXCL       121

#define SHOURNALK_MARK_W_INCL       130
#define SHOURNALK_MARK_W_EXCL       131


struct shounalk_settings {
    bool w_exclude_hidden;
    uint64_t w_max_event_count; /* stop collecting after # written files */

    bool r_only_writable;
    bool r_exclude_hidden;
    uint64_t r_max_event_count; /* stop collecting after # read files */

    /* only store the content of a read file, if... */
    bool r_store_only_writable; /* ...user also has write permission */
    uint32_t r_store_max_size; /* ...file-size is less or equal to max_size */
    uint16_t r_store_max_count_of_files; /* ...not already collected all desired files */
    bool r_store_exclude_hidden; /* ...it is not hidden */

    unsigned hash_max_count_reads; /* set to 0 to disable hash */
    unsigned hash_chunksize;
};

/// Mark specific paths of specific pid's (and their children)
/// for observation
struct shournalk_mark_struct {
    int pipe_fd; /* stats are written here after event processing finished */
    int target_fd; /* close events are written to this binary file */
    int flags; /* ADD, REMOVE, COMMIT */
    int action; /* PID, SCRIPT_INCL/EXCL */
    uint64_t pid;

    struct shounalk_settings settings;
    const void* data;
};

/// Close events are written to a binary file.
/// If 'bytes' is nonzero, the next N bytes are
/// the complete file content.
/// Followed by that is either the full filepath
/// or filename (null-terminated). In case of a filename, the event occurred
/// within the same directory as the previous event of the
/// given type (O_RDONLY, O_WRONLY)
struct shournalk_close_event {
    int flags; /* One of O_RDONLY, O_WRONLY, O_RDWR */
    uint64_t mtime; /* as unix timestamp */
    uint64_t size;
    uint64_t mode;
    uint64_t hash;
    bool hash_is_null;
    size_t bytes; /* read that many file-content-bytes next. */
    /* filename as null-terminated cstring */
};


/// When the observation finishes, this struct is written to
/// a pipe (created in user space) belonging to the notification
/// group
struct shournalk_run_result {
    int error_nb;
    uint64_t w_event_count;      /* # of logged write-events */
    uint64_t w_dropped_count;    /* # dropped exceeding max_event_count */
    uint64_t r_event_count;      /* # of logged read-events */
    uint64_t r_dropped_count;    /* # dropped exceeding max_event_count */
    uint32_t stored_event_count; /* number of (read) files in event target file */
    uint64_t lost_event_count;   /* if too many events occur, some may
                                    be dropped for performance reasons. */
    int selected_exitcode;       /* see SHOURNALK_MARK_COLLECT_EXITCODE */
};


#ifdef __cplusplus
}
#endif

