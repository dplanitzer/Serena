//
//  Object.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Object.h"

ErrorCode _Object_Create(ClassRef _Nonnull pClass, ByteCount instanceSize, ObjectRef _Nullable * _Nonnull pOutObject)
{
    decl_try_err();
    ObjectRef pObject;

    assert(instanceSize >= sizeof(Object));
    
    try(kalloc_cleared(instanceSize, (Byte**) &pObject));
    pObject->class = (Class*)pClass;
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
        if (self->class->deinit) {
            self->class->deinit(self);
        }
        kfree((Byte*) self);
    }
}
