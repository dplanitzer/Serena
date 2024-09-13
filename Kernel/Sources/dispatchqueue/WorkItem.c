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


void WorkItem_Init(WorkItemRef _Nonnull self, enum ItemType type, DispatchQueueClosure closure, bool isOwnedByQueue)
{
    SListNode_Init(&self->queue_entry);
    self->closure = closure;
    self->is_owned_by_queue = isOwnedByQueue;
    self->is_being_dispatched = false;
    self->cancelled = false;
    self->type = type;
}

// Creates a work item which will invoke the given closure. Note that work items
// are one-shot: they execute their closure and then the work item is destroyed.
errno_t WorkItem_Create_Internal(DispatchQueueClosure closure, bool isOwnedByQueue, WorkItemRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    WorkItemRef self;
    
    try(kalloc(sizeof(WorkItem), (void**) &self));
    WorkItem_Init(self, kItemType_Immediate, closure, isOwnedByQueue);
    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

// Creates a work item which will invoke the given closure. Note that work items
// are one-shot: they execute their closure and then the work item is destroyed.
// This is the creation method for parties that are external to the dispatch
// queue implementation.
errno_t WorkItem_Create(DispatchQueueClosure closure, WorkItemRef _Nullable * _Nonnull pOutSelf)
{
    return WorkItem_Create_Internal(closure, false, pOutSelf);
}

void WorkItem_Deinit(WorkItemRef _Nonnull self)
{
    SListNode_Deinit(&self->queue_entry);
    self->closure.func = NULL;
    self->closure.context = NULL;
    self->closure.isUser = false;
    self->completion = NULL;
    // Leave is_owned_by_queue alone
    AtomicBool_Set(&self->is_being_dispatched, false);
    self->cancelled = false;
}

// Deallocates the given work item.
void WorkItem_Destroy(WorkItemRef _Nullable self)
{
    if (self) {
        WorkItem_Deinit(self);
        kfree(self);
    }
}

// Sets the cancelled state of the given work item. The work item is marked as
// cancelled if the parameter is true and the cancelled state if cleared if the
// parameter is false. Note that it is the responsibility of the work item
// closure to check the cancelled state and to act appropriately on it.
// Clearing the cancelled state of a work item should normally not be necessary.
// The functionality exists to enable work item caching and reuse.
void WorkItem_SetCancelled(WorkItemRef _Nonnull self, bool flag)
{
    self->cancelled = flag;
}

// Returns true if the given work item is in cancelled state.
bool WorkItem_IsCancelled(WorkItemRef _Nonnull self)
{
    return self->cancelled;
}

// Signals the completion of a work item. State is protected by the dispatch
// queue lock. The 'isInterrupted' parameter indicates whether the item should
// be considered interrupted or finished.
void WorkItem_SignalCompletion(WorkItemRef _Nonnull self, bool isInterrupted)
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


void _Nullable Timer_Init(TimerRef _Nonnull self, TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, bool isOwnedByQueue)
{
    enum ItemType type = TimeInterval_Greater(interval, kTimeInterval_Zero) ? kItemType_RepeatingTimer : kItemType_OneShotTimer;

    WorkItem_Init((WorkItem*)self, type, closure, isOwnedByQueue);
    self->deadline = deadline;
    self->interval = interval;
}

// Creates a new timer. The timer will fire on or after 'deadline'. If 'interval'
// is greater than 0 then the timer will repeat until cancelled.
errno_t Timer_Create_Internal(TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, bool isOwnedByQueue, TimerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    TimerRef self;
    
    try(kalloc(sizeof(Timer), (void**) &self));
    Timer_Init(self, deadline, interval, closure, isOwnedByQueue);
    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

// Creates a new timer. The timer will fire on or after 'deadline'. If 'interval'
// is greater than 0 then the timer will repeat until cancelled.
// This is the creation method for parties that are external to the dispatch
// queue implementation.
errno_t Timer_Create(TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, TimerRef _Nullable * _Nonnull pOutSelf)
{
    return Timer_Create_Internal(deadline, interval, closure, false, pOutSelf);
}

void Timer_Destroy(TimerRef _Nullable self)
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
    decl_try_err();
    CompletionSignaler* self;
    
    try(kalloc(sizeof(CompletionSignaler), (void**) &self));
    CompletionSignaler_Init(self);
    Semaphore_Init(&self->semaphore, 0);
    *pOutComp = self;
    return EOK;

catch:
    *pOutComp = NULL;
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
