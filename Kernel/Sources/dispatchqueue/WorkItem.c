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

void WorkItem_Deinit(WorkItem* _Nonnull self)
{
    SListNode_Deinit(&self->queue_entry);
    self->closure.func = NULL;
    self->closure.context = NULL;
    self->closure.isUser = false;
    self->completion = NULL;
    self->tag = 0;

    if (self->timer) {
        Timer_Destroy(self->timer);
        self->timer = NULL;
    }
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

void Timer_Deinit(Timer* _Nonnull self)
{
    SListNode_Deinit(&self->queue_entry);
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
