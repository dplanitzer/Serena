//
//  HIDEventQueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/04/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "HIDEventQueue.h"
#include <kern/kalloc.h>
#include <machine/MonotonicClock.h>
#include <machine/Platform.h>
#include <sched/sem.h>


// The event queue stores events in a ring buffer with a size that is a
// power-of-2 number.
// See: <https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/>
typedef struct HIDEventQueue {
    sem_t       semaphore;
    uint8_t     capacity;
    uint8_t     capacityMask;
    uint8_t     readIdx;
    uint8_t     writeIdx;
    size_t      overflowCount;
    HIDEvent    data[1];
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
    sem_init(&self->semaphore, 0);
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
        sem_deinit(&self->semaphore);
        kfree(self);
    }
}

// Returns true if the queue is empty.
static inline bool HIDEventQueue_IsEmpty_Locked(HIDEventQueueRef _Nonnull self)
{
    return self->readIdx == self->writeIdx;
}

// Returns true if the queue is full.
static inline bool HIDEventQueue_IsFull_Locked(HIDEventQueueRef _Nonnull self)
{
    return self->writeIdx - self->readIdx == self->capacity;
}

// Returns the number of reports stored in the ring queue - aka the number of
// reports that can be read from the queue.
static inline int HIDEventQueue_ReadableCount_Locked(HIDEventQueueRef _Nonnull self)
{
    return self->writeIdx - self->readIdx;
}

// Returns the number of reports that can be written to the queue.
static inline int HIDEventQueue_WritableCount_Locked(HIDEventQueueRef _Nonnull self)
{
    return self->capacity - (self->writeIdx - self->readIdx);
}

// Returns true if the queue is empty.
bool HIDEventQueue_IsEmpty(HIDEventQueueRef _Nonnull self)
{
    const int irs = irq_disable();
    const bool r = HIDEventQueue_IsEmpty_Locked(self);
    irq_restore(irs);

    return r;
}

// Returns the number of times the queue overflowed. Note that the queue drops
// the oldest event every time it overflows.
size_t HIDEventQueue_GetOverflowCount(HIDEventQueueRef _Nonnull self)
{
    const int irs = irq_disable();
    const size_t overflowCount = self->overflowCount;
    irq_restore(irs);
    return overflowCount;
}

// Removes all events from the queue.
void HIDEventQueue_RemoveAll(HIDEventQueueRef _Nonnull self)
{
    const int irs = irq_disable();
    self->readIdx = 0;
    self->writeIdx = 0;
    irq_restore(irs);
}

// Posts the given event to the queue. This event replaces the oldest event
// in the queue if the queue is full. This function must be called from the
// interrupt context.
void HIDEventQueue_Put(HIDEventQueueRef _Nonnull self, HIDEventType type, const HIDEventData* _Nonnull pEventData)
{
    const int irs = irq_disable();

    if (HIDEventQueue_IsFull_Locked(self)) {
        // Queue is full - remove the oldest report
        self->readIdx = self->readIdx + 1;
        self->overflowCount++;
    }

    HIDEvent* pEvent = &self->data[self->writeIdx++ & self->capacityMask];
    pEvent->type = type;
    MonotonicClock_GetCurrentTime(gMonotonicClock, &pEvent->eventTime);
    pEvent->data = *pEventData;
    irq_restore(irs);

    sem_relinquish_irq(&self->semaphore);
}

// Removes the oldest event from the queue and returns a copy of it. Blocks the
// caller if the queue is empty. The caller stays blocked until either an event
// has arrived or 'timeout' has elapsed. Returns EOK if an event has been
// successfully dequeued or ETIMEDOUT if no event has arrived and the wait has
// timed out. Returns EAGAIN if timeout is 0 and no event is pending.
errno_t HIDEventQueue_Get(HIDEventQueueRef _Nonnull self, struct timespec timeout, HIDEvent* _Nonnull pOutEvent)
{
    decl_try_err();

    while (true) {
        const int irs = irq_disable();
        const bool hasEvent = !HIDEventQueue_IsEmpty_Locked(self);

        if (hasEvent) {
            *pOutEvent = self->data[self->readIdx++ & self->capacityMask];
        }
        irq_restore(irs);

        if (hasEvent) {
            err = EOK;
            break;
        }
        if (timeout.tv_sec <= 0 && timeout.tv_nsec <= 0) {
            err = EAGAIN;
            break;
        }

        err = sem_acquire(&self->semaphore, &timeout);
        if (err != EOK) {
            break;
        }
    }

    return err;
}
