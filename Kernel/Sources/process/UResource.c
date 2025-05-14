//
//  UResource.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "UResource.h"
#include <kern/kalloc.h>


typedef void (*UResource_Deinit_Impl)(void* _Nonnull self);

enum {
    kFlags_IsDeallocScheduled = 1
};


// Creates an instance of a UResource. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
errno_t UResource_AbstractCreate(Class* _Nonnull pClass, UResourceRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    UResourceRef self;

    try(kalloc_cleared(pClass->instanceSize, (void**) &self));
    self->super.clazz = pClass;
    Lock_Init(&self->countLock);
    self->useCount = 0;

catch:
    *pOutSelf = self;
    return err;
}

static void _UResource_Dealloc(UResourceRef _Nonnull self)
{
    UResource_Deinit_Impl pPrevDeinitImpl = NULL;
    Class* pCurClass = classof(self);

    for(;;) {
        UResource_Deinit_Impl pCurDeinitImpl = (UResource_Deinit_Impl)implementationof(deinit, UResource, pCurClass);
        
        if (pCurDeinitImpl != pPrevDeinitImpl) {
            pCurDeinitImpl(self);
            pPrevDeinitImpl = pCurDeinitImpl;
        }

        if (pCurClass == &kUResourceClass) {
            break;
        }

        pCurClass = pCurClass->super;
    }

    Lock_Deinit(&self->countLock);
    kfree(self);
}

void _UResource_Dispose(UResourceRef _Nullable self)
{
    bool doDealloc = false;

    Lock_Lock(&self->countLock);
    if (self->useCount > 0) {
        self->flags |= kFlags_IsDeallocScheduled;
    }
    else {
        doDealloc = true;
    }
    Lock_Unlock(&self->countLock);

    if (doDealloc) {
        // Can be triggered at most once. Thus no need to hold the lock while
        // running deallocation
        _UResource_Dealloc(self);
    }
}

void _UResource_BeginOperation(UResourceRef _Nonnull self)
{
    Lock_Lock(&self->countLock);
    self->useCount++;
    Lock_Unlock(&self->countLock);
}

void _UResource_EndOperation(UResourceRef _Nonnull self)
{
    bool doDealloc = false;

    Lock_Lock(&self->countLock);
    if (self->useCount >= 1) {
        self->useCount--;
        if (self->useCount == 0 && (self->flags & kFlags_IsDeallocScheduled) == kFlags_IsDeallocScheduled) {
            self->useCount = -1;
            doDealloc = true;
        }
    }
    Lock_Unlock(&self->countLock);

    if (doDealloc) {
        // Can be triggered at most once. Thus no need to hold the lock while
        // running deallocation
        _UResource_Dealloc(self);
    }
}

void UResource_deinit(UResourceRef _Nonnull self)
{
}


any_subclass_func_defs(UResource,
func_def(deinit, UResource)
);
