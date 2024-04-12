//
//  Object.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Object.h"
#include <klib/Memory.h>
#include <klib/Kalloc.h>
#ifndef __DISKIMAGE__
#include <klib/Log.h>

extern char _class;
extern char _eclass;
#endif


void Object_deinit(ObjectRef _Nonnull self)
{
}

ROOT_CLASS_METHODS(Object,
METHOD_IMPL(deinit, Object)
);

errno_t _Object_Create(ClassRef _Nonnull pClass, size_t extraByteCount, ObjectRef _Nullable * _Nonnull pOutObject)
{
    decl_try_err();
    ObjectRef pObject;

    if (extraByteCount > 0) {
        extraByteCount--;   // Account for the byte defined in the IOChannel structure
    }

    try(kalloc_cleared(pClass->instanceSize + extraByteCount, (void**) &pObject));
    pObject->clazz = pClass;
    pObject->retainCount = 1;
    *pOutObject = pObject;
    return EOK;

catch:
    *pOutObject = NULL;
    return err;
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
        ((ObjectMethodTable*)self->clazz->vtable)->deinit(self);
        kfree(self);
    }
}

bool _Object_InstanceOf(ObjectRef _Nonnull pObj, ClassRef _Nonnull pTargetClass)
{
    ClassRef curTargetClass = pTargetClass;
    ClassRef objClass = Object_GetClass(pObj);

    while (curTargetClass) {
        if (objClass == curTargetClass) {
            return true;
        }
        curTargetClass = curTargetClass->super;
    }

    return false;
}
