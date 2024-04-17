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
    Object_Release(self->dispatchQueue);
    self->dispatchQueue = NULL;
}

class_func_defs(UDispatchQueue, UResource,
override_func_def(deinit, UDispatchQueue, UResource)
);
