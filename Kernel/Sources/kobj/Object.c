//
//  Object.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Object.h"
#include <klib/Kalloc.h>

typedef void (*Object_Deinit_Impl)(void* _Nonnull self);


void Object_deinit(ObjectRef _Nonnull self)
{
}

any_subclass_func_defs(Object,
func_def(deinit, Object)
);


errno_t Object_Create(Class* _Nonnull pClass, size_t extraByteCount, void* _Nullable * _Nonnull pOutObject)
{
    decl_try_err();
    ObjectRef pObject;

    err = kalloc_cleared(pClass->instanceSize + extraByteCount, (void**) &pObject);
    if (err == EOK) {
        pObject->super.clazz = pClass;
        pObject->retainCount = 1;
    }
    *pOutObject = pObject;

    return err;
}

static void _Object_Dealloc(ObjectRef _Nonnull self)
{
    decl_try_err();
    Object_Deinit_Impl pPrevDeinitImpl = NULL;
    Class* pCurClass = classof(self);

    for(;;) {
        Object_Deinit_Impl pCurDeinitImpl = (Object_Deinit_Impl)implementationof(deinit, Object, pCurClass);
        
        if (pCurDeinitImpl != pPrevDeinitImpl) {
            pCurDeinitImpl(self);
            pPrevDeinitImpl = pCurDeinitImpl;
        }

        if (pCurClass == &kObjectClass) {
            break;
        }

        pCurClass = pCurClass->super;
    }

    kfree(self);
}

// Releases a strong reference on the given resource. Deallocates the resource
// when the reference count transitions from 1 to 0. Invokes the deinit method
// on the resource if the resource should be deallocated.
void _Object_Release(ObjectRef _Nullable self)
{
    if (self == NULL) {
        return;
    }

    const AtomicInt rc = AtomicInt_Decrement(&self->retainCount);

    // Note that we trigger the deallocation when the reference count transitions
    // from 1 to 0. The VP that caused this transition is the one that executes
    // the deallocation code. If another VP calls Release() while we are
    // deallocating the resource then nothing will happen. Most importantly no
    // second deallocation will be triggered. The reference count simply becomes
    // negative which is fine. In that sense a negative reference count signals
    // that the object is dead.
    if (rc == 0) {
        _Object_Dealloc(self);
    }
}
