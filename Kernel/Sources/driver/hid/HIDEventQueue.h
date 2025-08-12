//
//  HIDEventQueue.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/04/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDEventQueue_h
#define HIDEventQueue_h

#include <kern/errno.h>
#include <kern/types.h>
#include <kpi/hidevent.h>

struct HIDEventQueue;
typedef struct HIDEventQueue* HIDEventQueueRef;


// Allocates an empty event queue. 'capacity' is the queue capacity in terms of
// the maximum number of events it can store at the same time. This value is
// rounded up to the next power of 2.
extern errno_t HIDEventQueue_Create(size_t capacity, HIDEventQueueRef _Nullable * _Nonnull pOutSelf);

// Frees the event queue.
extern void HIDEventQueue_Destroy(HIDEventQueueRef _Nonnull self);

// Gets and sets the key repeat configuration. Key repeats are synthesized at
// HIDEventQueue_Get() time if needed.
extern void HIDEventQueue_GetKeyRepeatDelays(HIDEventQueueRef _Nonnull self, struct timespec* _Nullable pInitialDelay, struct timespec* _Nullable pRepeatDelay);
extern void HIDEventQueue_SetKeyRepeatDelays(HIDEventQueueRef _Nonnull self, const struct timespec* _Nonnull initialDelay, const struct timespec* _Nonnull repeatDelay);

// Returns the number of times the queue overflowed. Note that the queue drops
// the oldest event every time it overflows.
extern size_t HIDEventQueue_GetOverflowCount(HIDEventQueueRef _Nonnull self);

// Removes all events from the queue.
extern void HIDEventQueue_RemoveAll(HIDEventQueueRef _Nonnull self);

// Posts the given event to the queue. This report replaces the oldest event
// in the queue if the queue is full. This function must be called from the
// interrupt context.
extern void HIDEventQueue_Put(HIDEventQueueRef _Nonnull self, HIDEventType type, const HIDEventData* _Nonnull pEventData);

// Removes the oldest event from the queue and returns a copy of it. Blocks the
// caller if the queue is empty. The caller stays blocked until either an event
// has arrived or 'timeout' has elapsed. Returns EOK if an event has been
// successfully dequeued or ETIMEDOUT if no event has arrived and the wait has
// timed out.
extern errno_t HIDEventQueue_Get(HIDEventQueueRef _Nonnull self, const struct timespec* _Nonnull timeout, HIDEvent* _Nonnull pOutEvent);

#endif /* HIDEventQueue_h */
