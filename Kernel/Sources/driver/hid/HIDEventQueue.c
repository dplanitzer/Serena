//
//  HIDEventQueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/04/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "HIDEventQueue.h"
#include "HIDEventSynth.h"
#include <kern/kalloc.h>
#include <machine/clock.h>
#include <machine/irq.h>
#include <sched/cnd.h>


// The event queue stores events in a ring buffer with a size that is a
// power-of-2 number.
// See: <https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/>
typedef struct HIDEventQueue {
    mtx_t           mtx;
    cnd_t           cnd;

    HIDEventSynth   synth;

    uint16_t        capacity;
    uint16_t        capacityMask;
    uint16_t        readIdx;
    uint16_t        writeIdx;
    size_t          overflowCount;
    HIDEvent        data[1];
} HIDEventQueue;



// Allocates an empty event queue. 'capacity' is the queue capacity in terms of
// the maximum number of events it can store at the same time. This value is
// rounded up to the next power of 2.
errno_t HIDEventQueue_Create(size_t capacity, HIDEventQueueRef _Nullable * _Nonnull pOutSelf)
{
    assert(capacity >= 2);

    decl_try_err();
    const size_t powerOfTwoCapacity = siz_pow2_ceil(capacity);
    HIDEventQueue* self;
    
    assert(powerOfTwoCapacity <= UINT8_MAX/2);
    try(kalloc_cleared(sizeof(HIDEventQueue) + (powerOfTwoCapacity - 1) * sizeof(HIDEvent), (void**) &self));
    mtx_init(&self->mtx);
    cnd_init(&self->cnd);
    HIDEventSynth_Init(&self->synth);

    self->capacity = powerOfTwoCapacity;
    self->capacityMask = powerOfTwoCapacity - 1;
    self->readIdx = 0;
    self->writeIdx = 0;
    self->overflowCount = 0;

catch:
    *pOutSelf = self;
    return err;
}

// Frees the event queue.
void HIDEventQueue_Destroy(HIDEventQueueRef _Nonnull self)
{
    if (self) {
        HIDEventSynth_Deinit(&self->synth);
        cnd_deinit(&self->cnd);
        mtx_deinit(&self->mtx);
        kfree(self);
    }
}

void HIDEventQueue_GetKeyRepeatDelays(HIDEventQueueRef _Nonnull self, struct timespec* _Nullable pInitialDelay, struct timespec* _Nullable pRepeatDelay)
{
    mtx_lock(&self->mtx);
    if (pInitialDelay) {
        *pInitialDelay = self->synth.initialKeyRepeatDelay;
    }
    if (pRepeatDelay) {
        *pRepeatDelay = self->synth.keyRepeatDelay;
    }
    mtx_unlock(&self->mtx);
}

void HIDEventQueue_SetKeyRepeatDelays(HIDEventQueueRef _Nonnull self, const struct timespec* _Nonnull initialDelay, const struct timespec* _Nonnull repeatDelay)
{
    mtx_lock(&self->mtx);
    self->synth.initialKeyRepeatDelay = *initialDelay;
    self->synth.keyRepeatDelay = *repeatDelay;
    mtx_unlock(&self->mtx);
}

// Returns the number of reports stored in the ring queue - aka the number of
// reports that can be read from the queue.
#define READABLE_COUNT() \
(self->writeIdx - self->readIdx)

// Returns the number of reports that can be written to the queue.
#define WRITABLE_COUNT() \
(self->capacity - (self->writeIdx - self->readIdx))

// Returns the number of times the queue overflowed. Note that the queue drops
// the oldest event every time it overflows.
size_t HIDEventQueue_GetOverflowCount(HIDEventQueueRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const size_t overflowCount = self->overflowCount;
    mtx_unlock(&self->mtx);
    return overflowCount;
}

// Removes all events from the queue.
void HIDEventQueue_RemoveAll(HIDEventQueueRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    self->readIdx = 0;
    self->writeIdx = 0;
    mtx_unlock(&self->mtx);
}

// Posts the given event to the queue. This event replaces the oldest event
// in the queue if the queue is full. This function must be called from the
// interrupt context.
void HIDEventQueue_Put(HIDEventQueueRef _Nonnull self, HIDEventType type, did_t driverId, const HIDEventData* _Nonnull pEventData)
{
    mtx_lock(&self->mtx);
    if (WRITABLE_COUNT() > 0) {
        HIDEvent* pe = &self->data[self->writeIdx++ & self->capacityMask];
        pe->type = type;
        pe->driverId = driverId;
        clock_gettime(g_mono_clock, &pe->eventTime);
        pe->data = *pEventData;
    
        cnd_broadcast(&self->cnd);
    }
    else {
        self->overflowCount++;
    }
    mtx_unlock(&self->mtx);
}

// Removes the oldest event from the queue and returns a copy of it. Blocks the
// caller if the queue is empty. The caller stays blocked until either an event
// has arrived or 'timeout' has elapsed. Returns EOK if an event has been
// successfully dequeued or ETIMEDOUT if no event has arrived and the wait has
// timed out. Returns EAGAIN if timeout is 0 and no event is pending.
errno_t HIDEventQueue_Get(HIDEventQueueRef _Nonnull self, const struct timespec* _Nonnull timeout, HIDEvent* _Nonnull pOutEvent)
{
    decl_try_err();
    struct timespec deadline;
    HIDSynthResult ktr;

    mtx_lock(&self->mtx);
    for (;;) {
        const HIDEvent* queue_evt = NULL;

        if (READABLE_COUNT() > 0) {
            queue_evt = &self->data[self->readIdx & self->capacityMask];
        }


        const HIDSynthAction t = HIDEventSynth_Tick(&self->synth, queue_evt, &ktr);
        if (t == HIDSynth_UseEvent) {
            *pOutEvent = *queue_evt;
            self->readIdx++;
            break;
        }
        if (t == HIDSynth_MakeRepeat) {
            pOutEvent->type = kHIDEventType_KeyDown;
            pOutEvent->eventTime = ktr.deadline;
            pOutEvent->data.key.flags = ktr.flags;
            pOutEvent->data.key.keyCode = ktr.keyCode;
            pOutEvent->data.key.isRepeat = true;
            break;
        }

        if (t == HIDSynth_Wait) {
            deadline = *timeout;
        }
        else {
            deadline = (timespec_lt(&ktr.deadline, timeout)) ? ktr.deadline : *timeout;
        }
        if (deadline.tv_sec == 0 && deadline.tv_nsec == 0) {
            err = EAGAIN;
            break;
        }


        err = cnd_timedwait(&self->cnd, &self->mtx, &deadline);
        if (err == ETIMEDOUT) {
            struct timespec now;

            clock_gettime(g_mono_clock, &now);
            if (timespec_ge(&now, timeout)) {
                break;
            }
            err = EOK;
        }
        else if (err != EOK) {
            break;
        }
    }
    mtx_unlock(&self->mtx);

    return err;
}
