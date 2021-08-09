
#include <linux/circ_buf.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/cred.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/ioprio.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/mmu_context.h>
#include <linux/memcontrol.h>
#include <linux/mm_types.h>


#include "event_queue.h"
#include "event_consumer.h"
#include "shournal_kio.h"
#include "kutil.h"

#define __CONSUMER_JIFFY_OFFSET 200


/// "Consumes" the ringbuffer (writes tail)
/// @return the number of consumed bytes (*not* events).
static int
__consume_close_events(struct event_target* event_target){
    int bytes;
    int bytes_total;
    int head, tail;
    struct circ_buf* circ_buf = &event_target->event_consumer.circ_buf;
    const int cir_buf_size = event_target->event_consumer.circ_buf_size;
    struct close_event* e;
    unsigned long next_sched_jiffy;

    head = smp_load_acquire(&circ_buf->head);
    tail = READ_ONCE(circ_buf->tail);
    bytes_total = CIRC_CNT(head, tail, cir_buf_size);

    next_sched_jiffy =  jiffies + msecs_to_jiffies(__CONSUMER_JIFFY_OFFSET);
    for(bytes=0; bytes < bytes_total; ){
        e = (struct close_event*)&circ_buf->buf[tail];
        tail = (tail + sizeof (struct close_event)) & (cir_buf_size - 1);
        bytes += sizeof(struct close_event);

        close_event_consume(event_target, e);
        if(time_is_before_jiffies(next_sched_jiffy)){
            smp_store_release(&circ_buf->tail, tail);
            kutil_kthread_be_nice();
            next_sched_jiffy =  jiffies + msecs_to_jiffies(__CONSUMER_JIFFY_OFFSET);
        }
    }
    // maybe_todo: move into loop to avoid event-overflow?
    smp_store_release(&circ_buf->tail, tail);

    if( bytes_total > 0){
        // bulk refcount-decrement..
        int event_count = bytes_total/sizeof(struct close_event);
        event_target->consumed_event_count += event_count;
        if(atomic_sub_return(event_count, &event_target->_f_count) == 0 )
            __event_target_put(event_target);
    }
    return bytes_total;
}


/// For each event_target one thread is created, which
/// consumes the events of the target's ringbuffer.
int event_queue_consume_thread(void* data){
    struct event_target* event_target = (struct event_target*)data;
    struct event_consumer* consumer = &event_target->event_consumer;
    struct kbuffered_file* target_file = event_target->file;
    uint64_t schedcount = 0;
    int sleep_counter = 0;

    up(&consumer->start_sema);

    event_consumer_thread_setup(event_target);

    // lost wake-up problem.
    set_current_state(TASK_INTERRUPTIBLE);

    // Calling wake_up_process is costly, so we want the producer
    // to do so rarely. Therefore:
    // First loop until nothing can be consumed.
    // Then sleep and check again for new events, before scheduling
    // without timeout.
    while(!kthread_should_stop()){
        int consumed_bytes = __consume_close_events(event_target);
        if(consumed_bytes){
            sleep_counter = 0;
        } else {
            // maybe a good time to flush?
            if(target_file->__pos > target_file->__bufsize/4){
                event_consumer_flush_target_file_safe(event_target);
                // this might have taken a while, so..
                kutil_kthread_be_nice();
                continue;
            }

            if(sleep_counter > 2){
                smp_store_mb(event_target->event_consumer.woken_up, false);
                // By checking again for events we allow a
                // harmless race in the consumer.
                __consume_close_events(event_target);
                schedcount++;
                schedule();
            } else {
                schedule_timeout(1);
                sleep_counter++;
            }
            set_current_state(TASK_INTERRUPTIBLE);
        }

    }

    // consume final remaining events. Note in case *this* kthread
    // puts the final event_target-ref, we never get here.
    __consume_close_events(event_target);
    event_consumer_thread_cleanup(event_target);
    do_exit(0);
}


