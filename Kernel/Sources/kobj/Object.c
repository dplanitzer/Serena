//
//  Object.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Object.h"
#include <klib/Kalloc.h>

typedef void (*deinit_impl_t)(void* _Nonnull self);


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

void* _Nonnull Object_Retain(void* _Nonnull self)
{
    ObjectRef s = (ObjectRef)self;

    AtomicInt_Increment(&s->retainCount);
    return self;
}

static void _Object_Deinit(ObjectRef _Nonnull self)
{
    decl_try_err();
    deinit_impl_t pPrevDeinitImpl = NULL;
    Class* pCurClass = classof(self);

    for(;;) {
        deinit_impl_t pCurDeinitImpl = (deinit_impl_t)implementationof(deinit, Object, pCurClass);
        
        if (pCurDeinitImpl != pPrevDeinitImpl) {
            pCurDeinitImpl(self);
            pPrevDeinitImpl = pCurDeinitImpl;
        }

        if (pCurClass == class(Object)) {
            break;
        }

        pCurClass = pCurClass->super;
    }
}

// Releases a strong reference on the given resource. Deallocates the resource
// when the reference count transitions from 1 to 0. Invokes the deinit method
// on the resource if the resource should be deallocated.
void Object_Release(void* _Nullable self)
{
    ObjectRef s = (ObjectRef)self;

    if (s == NULL) {
        return;
    }

    const AtomicInt rc = AtomicInt_Decrement(&s->retainCount);

    // Note that we trigger the deallocation when the reference count transitions
    // from 1 to 0. The VP that caused this transition is the one that executes
    // the deallocation code. If another VP calls Release() while we are
    // deallocating the resource then nothing will happen. Most importantly no
    // second deallocation will be triggered. The reference count simply becomes
    // negative which is fine. In that sense a negative reference count signals
    // that the object is dead.
    if (rc == 0) {
        _Object_Deinit(s);
        kfree(s);
    }
}
