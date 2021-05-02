#include "shournalk_global.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tycho Kirchner");
MODULE_DESCRIPTION("Trace and collect metadata (path, hash, etc.) about "
                   "file close-events recursively for specific pid's");
MODULE_VERSION(SHOURNAL_VERSION);


#include "event_handler.h"
#include "event_handler.h"
#include "shournalk_sysfs.h"
#include "tracepoint_helper.h"
#include "event_queue.h"
#include "kutil.h"

#include "shournalk_test.h"

static int __init shournalk_init(void)
{
    int ret;

#ifdef DEBUG
    if(! run_tests()){
        return -EHOSTDOWN;
    }
#endif
    if((ret = (int)shournalk_global_constructor()) != 0){
        return ret;
    }
    if((ret = event_handler_constructor()) != 0)      goto error1;
    if ((ret = tracepoint_helper_constructor()) != 0) goto error2;
    if((ret = shournalk_sysfs_constructor()) != 0)    goto error3;

    return 0;

error3:
    tracepoint_helper_destructor();
error2:
    event_handler_destructor();
error1:
    shournalk_global_destructor();
    return ret;
}

static void __exit shournalk_exit(void)
{
    size_t target_count;

    // Be very careful about the order here.
    shournalk_sysfs_destructor();    
    tracepoint_helper_destructor();
    event_handler_destructor();
    shournalk_global_destructor();

    target_count = event_target_compute_count();

    if(target_count != 0){
        // We should never get here.
        pr_warn("event_target count not 0 but %ld\n", target_count);
        msleep(500);
        target_count = event_target_compute_count();
        if(target_count != 0){
            pr_warn("event_target count *still* not 0 but %ld\n", target_count);
        }
        // maybe_todo: __event_target_put. But this will likely
        // not be enough.
    }
}

module_init(shournalk_init)
module_exit(shournalk_exit)


