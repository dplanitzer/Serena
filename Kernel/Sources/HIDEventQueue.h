//
//  HIDEventQueue.h
//  Apollo
//
//  Created by Dietmar Planitzer on 10/04/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDEventQueue_h
#define HIDEventQueue_h

#include <klib/klib.h>
#include "HIDEvent.h"

struct _HIDEventQueue;
typedef struct _HIDEventQueue* HIDEventQueueRef;


// Allocates an empty event queue. 'capacity' is the queue capacity in terms of
// the maximum number of events it can store at the same time. This value is
// rounded up to the next power of 2.
extern errno_t HIDEventQueue_Create(int capacity, HIDEventQueueRef _Nullable * _Nonnull pOutQueue);

// Frees the event queue.
extern void HIDEventQueue_Destroy(HIDEventQueueRef _Nonnull pQueue);

// Returns true if the queue is empty.
bool HIDEventQueue_IsEmpty(HIDEventQueueRef _Nonnull pQueue);

// Returns the number of times the queue overflowed. Note that the queue drops
// the oldest event every time it overflows.
extern int HIDEventQueue_GetOverflowCount(HIDEventQueueRef _Nonnull pQueue);

// Removes all events from the queue.
extern void HIDEventQueue_RemoveAll(HIDEventQueueRef _Nonnull pQueue);

// Posts the given event to the queue. This report replaces the oldest event
// in the queue if the queue is full. This function must be called from the
// interrupt context.
extern void HIDEventQueue_Put(HIDEventQueueRef _Nonnull pQueue, HIDEventType type, const HIDEventData* _Nonnull pEventData);

// Removes the oldest event from the queue and returns a copy of it. Blocks the
// caller if the queue is empty. The caller stays blocked until either an event
// has arrived or 'timeout' has elapsed. Returns EOK if an event has been
// successfully dequeued or ETIMEDOUT if no event has arrived and the wait has
// timed out.
extern errno_t HIDEventQueue_Get(HIDEventQueueRef _Nonnull pQueue, HIDEvent* _Nonnull pOutEvent, TimeInterval timeout);

#endif /* HIDEventQueue_h */
