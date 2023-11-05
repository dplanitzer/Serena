//
//  HIDEventQueue.c
//  Apollo
//
//  Created by Dietmar Planitzer on 10/04/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "HIDEventQueue.h"
#include "MonotonicClock.h"
#include "Semaphore.h"

// The event queue stores events in a ring buffer with a size that is a
// power-of-2 number.
// See: <https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/>
typedef struct _HIDEventQueue {
    Semaphore   semaphore;
    UInt8       capacity;
    UInt8       capacityMask;
    UInt8       readIdx;
    UInt8       writeIdx;
    Int         overflowCount;
    HIDEvent    data[1];
} HIDEventQueue;



// Allocates an empty event queue. 'capacity' is the queue capacity in terms of
// the maximum number of events it can store at the same time. This value is
// rounded up to the next power of 2.
ErrorCode HIDEventQueue_Create(Int capacity, HIDEventQueueRef _Nullable * _Nonnull pOutQueue)
{
    assert(capacity >= 2);

    decl_try_err();
    const Int powerOfTwoCapacity = Int_NextPowerOf2(capacity);
    HIDEventQueue* pQueue;
    
    assert(powerOfTwoCapacity <= UINT8_MAX/2);
    try(kalloc_cleared(sizeof(HIDEventQueue) + (powerOfTwoCapacity - 1) * sizeof(HIDEvent), (Byte**)&pQueue));
    Semaphore_Init(&pQueue->semaphore, 0);
    pQueue->capacity = powerOfTwoCapacity;
    pQueue->capacityMask = powerOfTwoCapacity - 1;
    pQueue->readIdx = 0;
    pQueue->writeIdx = 0;
    pQueue->overflowCount = 0;

    *pOutQueue = pQueue;
    return EOK;

catch:
    *pOutQueue = NULL;
    return err;
}

// Frees the event queue.
void HIDEventQueue_Destroy(HIDEventQueueRef _Nonnull pQueue)
{
    if (pQueue) {
        Semaphore_Deinit(&pQueue->semaphore);
        kfree((Byte*) pQueue);
    }
}

// Returns true if the queue is empty.
static inline Bool HIDEventQueue_IsEmpty_Locked(HIDEventQueueRef _Nonnull pQueue)
{
    return pQueue->readIdx == pQueue->writeIdx;
}

// Returns true if the queue is full.
static inline Bool HIDEventQueue_IsFull_Locked(HIDEventQueueRef _Nonnull pQueue)
{
    return pQueue->writeIdx - pQueue->readIdx == pQueue->capacity;
}

// Returns the number of reports stored in the ring queue - aka the number of
// reports that can be read from the queue.
static inline Int HIDEventQueue_ReadableCount_Locked(HIDEventQueueRef _Nonnull pQueue)
{
    return pQueue->writeIdx - pQueue->readIdx;
}

// Returns the number of reports that can be written to the queue.
static inline Int HIDEventQueue_WritableCount_Locked(HIDEventQueueRef _Nonnull pQueue)
{
    return pQueue->capacity - (pQueue->writeIdx - pQueue->readIdx);
}

// Returns true if the queue is empty.
Bool HIDEventQueue_IsEmpty(HIDEventQueueRef _Nonnull pQueue)
{
    const Int irs = cpu_disable_irqs();
    const Bool r = HIDEventQueue_IsEmpty_Locked(pQueue);
    cpu_restore_irqs(irs);

    return r;
}

// Returns the number of times the queue overflowed. Note that the queue drops
// the oldest event every time it overflows.
Int HIDEventQueue_GetOverflowCount(HIDEventQueueRef _Nonnull pQueue)
{
    const Int irs = cpu_disable_irqs();
    const Int overflowCount = pQueue->overflowCount;
    cpu_restore_irqs(irs);
    return overflowCount;
}

// Removes all events from the queue.
void HIDEventQueue_RemoveAll(HIDEventQueueRef _Nonnull pQueue)
{
    const Int irs = cpu_disable_irqs();
    pQueue->readIdx = 0;
    pQueue->writeIdx = 0;
    cpu_restore_irqs(irs);
}

// Posts the given event to the queue. This event replaces the oldest event
// in the queue if the queue is full. This function must be called from the
// interrupt context.
void HIDEventQueue_Put(HIDEventQueueRef _Nonnull pQueue, HIDEventType type, const HIDEventData* _Nonnull pEventData)
{
    const Int irs = cpu_disable_irqs();

    if (HIDEventQueue_IsFull_Locked(pQueue)) {
        // Queue is full - remove the oldest report
        pQueue->readIdx = pQueue->readIdx + 1;
        pQueue->overflowCount++;
    }

    HIDEvent* pEvent = &pQueue->data[pQueue->writeIdx++ & pQueue->capacityMask];
    pEvent->type = type;
    pEvent->eventTime = MonotonicClock_GetCurrentTime();
    pEvent->data = *pEventData;
    cpu_restore_irqs(irs);

    Semaphore_ReleaseFromInterruptContext(&pQueue->semaphore);
}

// Removes the oldest event from the queue and returns a copy of it. Blocks the
// caller if the queue is empty. The caller stays blocked until either an event
// has arrived or 'timeout' has elapsed. Returns EOK if an event has been
// successfully dequeued or ETIMEDOUT if no event has arrived and the wait has
// timed out.
ErrorCode HIDEventQueue_Get(HIDEventQueueRef _Nonnull pQueue, HIDEvent* _Nonnull pOutEvent, TimeInterval timeout)
{
    decl_try_err();
    const Int irs = cpu_disable_irqs();

    while (true) {
        // This implicitly and temporarily reenables IRQs while we are blocked
        // waiting. IRQs are disabled once again when this call comes back.
        try(Semaphore_Acquire(&pQueue->semaphore, timeout));

        if (!HIDEventQueue_IsEmpty_Locked(pQueue)) {
            *pOutEvent = pQueue->data[pQueue->readIdx++ & pQueue->capacityMask];
            break;
        }
    }
    cpu_restore_irqs(irs);

    return EOK;

catch:
    cpu_restore_irqs(irs);
    return err;
}
