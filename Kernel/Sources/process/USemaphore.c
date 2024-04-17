//
//  USemaphore.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "USemaphore.h"


errno_t USemaphore_Create(int npermits, USemaphoreRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    USemaphoreRef self;

    try(UResource_AbstractCreate(&kUSemaphoreClass, (UResourceRef*)&self));
    Semaphore_Init(&self->sema, npermits);
    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void USemaphore_deinit(USemaphoreRef _Nonnull self)
{
    Semaphore_Deinit(&self->sema);
}

class_func_defs(USemaphore, UResource,
override_func_def(deinit, USemaphore, UResource)
);
