
#include <linux/init.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fdtable.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "shournalk_sysfs.h"
#include "shournalk_user.h"
#include "event_handler.h"
#include "event_target.h"
#include "kutil.h"

// Use «default attribute groups». Kernel v5.1-rc3,
// aa30f47cf666111f6bbfd15f290a27e8a7b9d854 added default attribute groups
// while v5.18-rc1, cdb4f26a63c391317e335e6e683a614358e70aeb
// dropped legacy support. So we switch somewhere in the middle.
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 8, 0))
#define SHOURNALK_USE_ATTR_GROUPS
#endif

struct shournal_obj {
    struct kobject kobj;
};
#define to_shournal_obj(x) container_of(x, struct shournal_obj, kobj)

struct shournal_attr {
    struct attribute attr;
    ssize_t (*show)(struct shournal_obj*, struct shournal_attr*, char*);
    ssize_t (*store)(struct shournal_obj*, struct shournal_attr*, const char*, size_t);
};
#define to_shournal_attr(x) container_of(x, struct shournal_attr, attr)


/// Entry point for all registered show-functions
static ssize_t shournal_attr_show(struct kobject *kobj,
                 struct attribute *attr,
                 char *buf)
{
    struct shournal_attr *attribute;
    struct shournal_obj *o;
    attribute = to_shournal_attr(attr);
    o = to_shournal_obj(kobj);
    if (!attribute->show)
        return -EIO;
    return attribute->show(o, attribute, buf);
}


/// Entry point for all registered store-functions
static ssize_t shournal_attr_store(struct kobject *kobj,
                  struct attribute *attr,
                  const char *buf, size_t len)
{
    struct shournal_attr *attribute;
    struct shournal_obj *o;
    attribute = to_shournal_attr(attr);
    o = to_shournal_obj(kobj);
    if (!attribute->store)
        return -EIO;
    return attribute->store(o, attribute, buf, len);
}

static struct sysfs_ops shournalk_ops = {
    .show = shournal_attr_show,
    .store = shournal_attr_store,
};


static ssize_t
__mark(struct shournal_obj*, struct shournal_attr*, const char*, size_t);
static ssize_t __show_version(struct shournal_obj *o __attribute__ ((unused)),
                              struct shournal_attr* attr __attribute__ ((unused)),
                              char *buf)
{
    return sprintf(buf, SHOURNAL_VERSION);
}

static struct shournal_attr attr_mark = __ATTR(mark, 0664, NULL, __mark);
static struct shournal_attr attr_version = __ATTR(version, 0444, __show_version, NULL);


static struct attribute *shournal_default_attrs[] = {
    &attr_mark.attr,
    &attr_version.attr,
    NULL,
};
#ifdef SHOURNALK_USE_ATTR_GROUPS
ATTRIBUTE_GROUPS(shournal_default);
#endif


static void shournal_obj_release(struct kobject *kobj){
    struct shournal_obj* o;
    o = to_shournal_obj(kobj);
    kfree(o);
}

static struct kobj_type shournal_kobj_ktype = {
    .sysfs_ops	= &shournalk_ops,
#ifdef SHOURNALK_USE_ATTR_GROUPS
    .default_groups = shournal_default_groups,
#else
    .default_attrs = (struct attribute **)&shournal_default_attrs,
#endif
    .release = shournal_obj_release,
};

static struct kset *shournal_kset;
static struct shournal_obj *shournal_obj;

/// Create kset and kobject and register our attribute function(s).
/// kset *must* be created and set for kobject_uevent.
/// See also: samples/kobject/kset-example.c
int shournalk_sysfs_constructor(void){
    int ret;

    shournal_kset = kset_create_and_add("shournalk_root", NULL, kernel_kobj);
    if (!shournal_kset){
        return -ENOMEM;
    }

    shournal_obj = kzalloc(sizeof(*shournal_obj), GFP_KERNEL);
    if(! shournal_obj){
        ret = -ENOMEM;
        goto err_shournal_obj_alloc;
    }
    shournal_obj->kobj.kset = shournal_kset;

    ret = kobject_init_and_add(&shournal_obj->kobj, &shournal_kobj_ktype, NULL,
                               "%s", "shournalk_ctrl");
    if (ret){
        pr_err("kobject_init_and_add failed");
        goto err_shournal_obj_add;
    }

    if((ret=kobject_uevent(&shournal_obj->kobj, KOBJ_ADD) )) {
        pr_warn("kobject_uevent failed\n");
        goto err_shournal_obj_add;
    }

    return 0;

err_shournal_obj_add:
    kobject_put(&shournal_obj->kobj);
err_shournal_obj_alloc:
    kset_unregister(shournal_kset);

    return ret;
}


void shournalk_sysfs_destructor(void){    
    kobject_put(&shournal_obj->kobj);
    kset_unregister(shournal_kset);
}


//////////////////////////////////////////////////////////////////////


static long verify_hash_settings(struct shournalk_mark_struct * mark_struct){
    if(mark_struct->settings.hash_max_count_reads == 0){
        // hash is disabled. Be safe and also set chunksize to 0
        mark_struct->settings.hash_chunksize = 0;
        return 0;
    }
    // maybe_todo: remove the upper limit: partial hashing can handle this
    // by digesting the max chunksize and *not* seeking afterwards.
    if(mark_struct->settings.hash_chunksize < 8 ||
       mark_struct->settings.hash_chunksize > PART_HASH_MAX_CHUNKSIZE){
        pr_debug("Invalid hashsettings. Chunksize must be:"
                 " between 8 and %d bytes\n", PART_HASH_MAX_CHUNKSIZE);
        return -EINVAL;
    }

    if(mark_struct->settings.hash_max_count_reads < 1 ||
       mark_struct->settings.hash_max_count_reads > 128){
        pr_debug("Invalid hashsettings. Max count of reads "
                 "must be between 1 and 128\n");
        return -EINVAL;
    }

    return 0;
}

static long __handle_pid_add(struct shournalk_mark_struct* mark_struct){
    pid_t pid = (pid_t)mark_struct->pid;
    struct event_target* event_target;
    // somewhat arbitrary limits
    const int STORE_MAX_SIZE = 1024*1024 * 2;
    const int STORE_MAX_FILECOUNT = 100;
    long ret;
    bool collect_exitcode = mark_struct->flags & SHOURNALK_MARK_COLLECT_EXITCODE;

    if((ret = verify_hash_settings(mark_struct))){
        return ret;
    }
    if(mark_struct->settings.r_store_max_size > STORE_MAX_SIZE){
        pr_debug("r_store_max_size > %d\n", STORE_MAX_SIZE);
        return -EINVAL;
    }
    if(mark_struct->settings.r_store_max_count_of_files > STORE_MAX_FILECOUNT){
        pr_debug("r_store_max_count_of_files > %d\n", STORE_MAX_FILECOUNT);
        return -EINVAL;
    }

    if(mark_struct->settings.w_max_event_count == 0 ||
            mark_struct->settings.r_max_event_count == 0){
        pr_debug("max_event_count(s) must not be zero\n");
        return -EINVAL;
    }

    event_target = event_target_get_or_create(mark_struct);
    if(IS_ERR(event_target)){
        return PTR_ERR(event_target);
    }
    ret = event_handler_add_pid(event_target, pid, collect_exitcode);

    event_target_put(event_target);
    return ret;
}


static long __handle_pid_remove(const struct shournalk_mark_struct * mark_struct){
    pid_t pid = (pid_t)mark_struct->pid;
    return event_handler_remove_pid(pid);
}

/// @return length of passed string or neg. error
static ssize_t __copy_path_from_user(char* buf, const char* __user src){
   long str_len = strncpy_from_user(buf, src, PATH_MAX);
   if(str_len <= 0){
       pr_debug("strncpy_from_user returned %ld\n", str_len);
       if(str_len < 0) return str_len;
       return -EINVAL;
   }
   // we read something. While not a real sanity check, at least check for
   // leading /
   if(unlikely(buf[0] != '/')){
       return -EINVAL;
   }
   return str_len;
}

/// If paths were not finalized yet, add param src to
/// param pathtree
static long __add_user_path(struct kpathtree* pathtree, const char* __user src){
    long ret = 0;
    char* path_tmp;

    path_tmp = kzalloc(PATH_MAX, SHOURNALK_GFP);
    if(!path_tmp) return -ENOMEM;

    if((ret = __copy_path_from_user(path_tmp, src)) < 0){
         goto out;
    }
    ret = kpathtree_add(pathtree, path_tmp, (int)ret);

out:
    kfree(path_tmp);
    return ret;
}

static ssize_t
__copy_file_extensions_from_user(char* buf, size_t buf_len, const char* __user src){
   long str_len = strncpy_from_user(buf, src, buf_len);
   if(str_len <= 0){
       pr_debug("strncpy_from_user returned %ld\n", str_len);
       if(str_len < 0) return str_len;
       return -EINVAL;
   }
   // shortes possible allowed extension, inluding trailing /
   // is e.g. o/
   if(unlikely(str_len < 2)){
       pr_debug("received extensions too short\n");
       return -EINVAL;
   }
   return str_len;
}

/// If paths were not finalized yet, add param src to
/// param pathtree
static long __add_user_file_extensions(struct file_extensions* exts, const char* __user src){
    long ret = 0;
    char* ext_tmp;

    ext_tmp = kzalloc(PAGE_SIZE, SHOURNALK_GFP);
    if(!ext_tmp) return -ENOMEM;

    if((ret = __copy_file_extensions_from_user(ext_tmp, PAGE_SIZE, src)) < 0){
         goto out;
    }
    ret = file_extensions_add_multiple(exts, ext_tmp, (int)ret);

out:
    kfree(ext_tmp);
    return ret;
}


static long __handle_mark_add(struct shournalk_mark_struct mark_struct){
    long ret = -EINVAL;
    struct event_target* t;
    if(mark_struct.action == SHOURNALK_MARK_PID){
        return __handle_pid_add(&mark_struct);
    }

    // for all other add-actions an existing event target is required
    t = event_target_get_existing(&mark_struct);
    if(IS_ERR(t)){
        return PTR_ERR(t);
    }

    // locking applies to not committed targets only, so it is
    // no problem to lock (a bit) early
    mutex_lock(&t->lock);
    if(event_target_is_commited(t)){
        pr_debug("invalid action %d - "
                 "event-target is already committed", mark_struct.action);
        ret = -EBUSY;
        goto unlock_put;
    }

    switch (mark_struct.action) {
    case SHOURNALK_MARK_W_INCL:
        ret = __add_user_path(&t->w_includes, mark_struct.data); break;
    case SHOURNALK_MARK_W_EXCL:
        ret = __add_user_path(&t->w_excludes, mark_struct.data); break;
    case SHOURNALK_MARK_R_INCL:
        ret = __add_user_path(&t->r_includes, mark_struct.data); break;
    case SHOURNALK_MARK_R_EXCL:
        ret = __add_user_path(&t->r_excludes, mark_struct.data); break;
    case SHOURNALK_MARK_SCRIPT_INCL:
        ret = __add_user_path(&t->script_includes, mark_struct.data); break;
    case SHOURNALK_MARK_SCRIPT_EXCL:
        ret = __add_user_path(&t->script_excludes, mark_struct.data); break;
    case SHOURNALK_MARK_SCRIPT_EXTS:
        ret = __add_user_file_extensions(&t->script_ext, mark_struct.data); break;
    default:
        ret = -EINVAL;
    }

unlock_put:
    mutex_unlock(&t->lock);
    event_target_put(t);
    return ret;
}


static long __handle_mark_remove(struct shournalk_mark_struct mark_struct){
    long ret;
    switch (mark_struct.action) {
    case SHOURNALK_MARK_PID:
        ret = __handle_pid_remove(&mark_struct); break;
    default:
        ret = -EINVAL;
    }
    return ret;
}

static long __handle_commit(struct shournalk_mark_struct mark_struct){
    long ret = 0;
    struct event_target* event_target;
    if( IS_ERR(event_target = event_target_get_existing(&mark_struct))) {
        return PTR_ERR(event_target);
    }
    mutex_lock(&event_target->lock);
    ret = event_target_commit(event_target);
    mutex_unlock(&event_target->lock);

    event_target_put(event_target);
    return ret;
}

static ssize_t __mark(struct shournal_obj* obj  __attribute__ ((unused)),
                      struct shournal_attr* attr  __attribute__ ((unused)),
                      const char* buf, size_t count){
    const struct shournalk_mark_struct * s_mark;
    ssize_t ret = 0;

    if(count != sizeof (struct shournalk_mark_struct)){
        return -EILSEQ;
    }

    s_mark = (const struct shournalk_mark_struct*) buf;

    if(s_mark->flags & SHOURNALK_MARK_ADD)
        ret = __handle_mark_add(*s_mark);
    else if(s_mark->flags & SHOURNALK_MARK_REMOVE)
        ret = __handle_mark_remove(*s_mark);
    else if(s_mark->flags & SHOURNALK_MARK_COMMIT)
        ret = __handle_commit(*s_mark);
    else
        ret = -EINVAL;

    if(ret != 0){
        WARN_ONCE(ret > 0, "pos. error received");
        return ret;
    }
    return count;
}



