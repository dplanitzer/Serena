//
//  WorkItem.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/27/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "DispatchQueuePriv.h"

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Work Items

void WorkItem_Init(WorkItem* _Nonnull self, enum ItemType type, DispatchQueueClosure closure, uintptr_t tag, bool isOwnedByQueue)
{
    SListNode_Init(&self->queue_entry);
    self->closure = closure;
    self->tag = tag;
    self->is_owned_by_queue = isOwnedByQueue;
    self->type = type;
}

// Creates a work item which will invoke the given closure. Note that work items
// are one-shot: they execute their closure and then the work item is destroyed.
errno_t WorkItem_Create(DispatchQueueClosure closure, bool isOwnedByQueue, WorkItem* _Nullable * _Nonnull pOutSelf)
{
    WorkItem* self;
    const errno_t err = kalloc(sizeof(WorkItem), (void**) &self);

    if (err == EOK) {
        WorkItem_Init(self, kItemType_Immediate, closure, 0, isOwnedByQueue);
    }
    *pOutSelf = self;
    return err;
}

void WorkItem_Deinit(WorkItem* _Nonnull self)
{
    SListNode_Deinit(&self->queue_entry);
    self->closure.func = NULL;
    self->closure.context = NULL;
    self->closure.isUser = false;
    self->completion = NULL;
    self->tag = 0;
    // Leave is_owned_by_queue alone
}

// Deallocates the given work item.
void WorkItem_Destroy(WorkItem* _Nullable self)
{
    if (self) {
        WorkItem_Deinit(self);
        kfree(self);
    }
}

// Signals the completion of a work item. State is protected by the dispatch
// queue lock. The 'isInterrupted' parameter indicates whether the item should
// be considered interrupted or finished.
void WorkItem_SignalCompletion(WorkItem* _Nonnull self, bool isInterrupted)
{
    if (self->completion != NULL) {
        self->completion->isInterrupted = isInterrupted;
        Semaphore_Relinquish(&self->completion->semaphore);
        self->completion = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Timers

void _Nullable Timer_Init(Timer* _Nonnull self, TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, uintptr_t tag, bool isOwnedByQueue)
{
    enum ItemType type = (TimeInterval_Greater(interval, kTimeInterval_Zero) && !TimeInterval_Equals(interval, kTimeInterval_Infinity)) ? kItemType_RepeatingTimer : kItemType_OneShotTimer;

    WorkItem_Init((WorkItem*)self, type, closure, tag, isOwnedByQueue);
    self->deadline = deadline;
    self->interval = interval;
}

// Creates a new timer. The timer will fire on or after 'deadline'. If 'interval'
// is greater than 0 then the timer will repeat until removed.
errno_t Timer_Create(TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, bool isOwnedByQueue, Timer* _Nullable * _Nonnull pOutSelf)
{
    Timer* self;    
    const errno_t err = kalloc(sizeof(Timer), (void**) &self);

    if (err == EOK) {
        Timer_Init(self, deadline, interval, closure, 0, isOwnedByQueue);
    }
    *pOutSelf = self;
    return err;
}

void Timer_Destroy(Timer* _Nullable self)
{
    if (self) {
        Timer_Deinit(self);
        kfree(self);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Completion Signalers

void CompletionSignaler_Init(CompletionSignaler* _Nonnull self)
{
    SListNode_Init(&self->queue_entry);
    self->isInterrupted = false;
}

// Creates a completion signaler.
errno_t CompletionSignaler_Create(CompletionSignaler* _Nullable * _Nonnull pOutComp)
{
    CompletionSignaler* self;
    const errno_t err = kalloc(sizeof(CompletionSignaler), (void**) &self);

    if (err == EOK) {
        CompletionSignaler_Init(self);
        Semaphore_Init(&self->semaphore, 0);
    }
    *pOutComp = self;
    return err;
}

void CompletionSignaler_Deinit(CompletionSignaler* _Nonnull self)
{
    SListNode_Deinit(&self->queue_entry);
    self->isInterrupted = false;
}

// Deallocates the given completion signaler.
void CompletionSignaler_Destroy(CompletionSignaler* _Nullable self)
{
    if (self) {
        CompletionSignaler_Deinit(self);
        Semaphore_Deinit(&self->semaphore);
        kfree(self);
    }
}
