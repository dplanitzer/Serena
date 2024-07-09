//
//  UDispatchQueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/16/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "UDispatchQueue.h"


errno_t UDispatchQueue_Create(int minConcurrency, int maxConcurrency, int qos, int priority, VirtualProcessorPoolRef _Nonnull vpPoolRef, ProcessRef _Nullable _Weak pProc, UDispatchQueueRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    UDispatchQueueRef self;

    try(UResource_AbstractCreate(&kUDispatchQueueClass, (UResourceRef*)&self));
    try(DispatchQueue_Create(minConcurrency, maxConcurrency, qos, priority, vpPoolRef, pProc, &self->dispatchQueue));
    *pOutSelf = self;
    return EOK;

catch:
    UResource_Dispose(self);
    *pOutSelf = NULL;
    return err;
}

void UDispatchQueue_deinit(UDispatchQueueRef _Nonnull self)
{
    // Calling destroy() on a dispatch queue should terminate the queue in any
    // case. We then drop our reference to the queue. However, keep in mind that
    // some other portion of the kernel may still hold another reference to the
    // queue which keeps it alive. This is okay. User space though has declared
    // that it no longer wants this queue to execute anything and thus we move
    // it to terminated state here. This ensures that if some other kernel portion
    // attempts to dispatch something to the queue that this dispatch will fail
    // with an appropriate error and that no new code will run on the queue. Once
    // the last kernel portion holding a reference to the queue releases this
    // reference, the queue will get destroyed for good.
    DispatchQueue_Terminate(self->dispatchQueue);
    Object_Release(self->dispatchQueue);
    self->dispatchQueue = NULL;
}

class_func_defs(UDispatchQueue, UResource,
override_func_def(deinit, UDispatchQueue, UResource)
);
