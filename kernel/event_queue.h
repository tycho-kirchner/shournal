/* File events are stored into a ringbuffer
 * and consumed in a per-event_target-thread.
 */

#pragma once

#include "shournalk_global.h"

#include <linux/mount.h>

#include "event_target.h"
#include "event_consumer.h"


int event_queue_consume_thread(void *data);

/// Threadsafe enqueue the close event and wake up
/// the consumer. This function consumes one event_target-reference (passes
/// it to the consumer or puts it in case of overflow)!
static inline void event_queue_add(struct event_target* event_target, struct file* file){
    int head;
    int tail;
    int remaining_bytes;
    struct close_event* close_ev;
    struct event_consumer* consumer = &event_target->event_consumer;
    struct circ_buf* circ_buf = &consumer->circ_buf;

    // Be optimistic, that we have space in the ringbuf. We
    // *must* get the refs before enqueuing, otherwise
    // the consumer might put the last ones before us!
    mntget(file->f_path.mnt);
    dget(file->f_path.dentry);

    // No need to ihold(dentry->d_inode)
    // "as long as a counted reference is held to a dentry,
    //  a non-NULL ->d_inode value will never be changed."
    // See also: kernel.org/doc/html/latest/filesystems/path-lookup.html

    spin_lock(&consumer->queue_lock);
    head = READ_ONCE(circ_buf->head);
    tail = READ_ONCE(circ_buf->tail);
    remaining_bytes = CIRC_SPACE(head ,tail ,consumer->circ_buf_size);

    if (unlikely(remaining_bytes < (int)sizeof (struct close_event))) {
        unsigned long long lostcount = READ_ONCE(event_target->lost_event_count);
        ++lostcount;
        // Event is lost, consumer was too slow.
        pr_devel("too many file events - skipping some (now lost: %lld)\n", lostcount);
        WRITE_ONCE(event_target->lost_event_count, lostcount);
        goto overflow_out;
    }
    close_ev = (struct close_event*)&circ_buf->buf[head];
    close_ev->f_mode = file->f_mode;
    close_ev->path = file->f_path;

    // write new head *after* having written content:
    head = (head + sizeof (struct close_event)) & (consumer->circ_buf_size - 1);
    smp_store_release(&circ_buf->head, head);

    spin_unlock(&consumer->queue_lock);

    // We could simply call wake_up_process all the time, but this
    // slows down things significantly when many events occur.
    // Therefore try to only wake the consumer up, if
    // necessary.
    // The consumer sets woken_up to false and
    // afterwards checks for remaining events. Due to this race
    // it may (rarely) happen that we wake the consumer up
    // with nothing to do, *but* it can *never* happen
    // that we produce something which is never consumed.
    if(READ_ONCE(consumer->woken_up))
        return;

    smp_store_mb(consumer->woken_up, true);
    wake_up_process(consumer->consume_task);

    return;

overflow_out:
    spin_unlock(&consumer->queue_lock);
    dput(file->f_path.dentry);
    mntput(file->f_path.mnt);
    event_target_put(event_target);
}

