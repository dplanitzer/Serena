//
//  Object.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Object.h"
#include <klib/Bytes.h>
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

errno_t _Object_Create(ClassRef _Nonnull pClass, ssize_t extraByteCount, ObjectRef _Nullable * _Nonnull pOutObject)
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

// Assigns the object reference 'pNewObject' to 'pOldObject' by retaining
// 'pNewObject' and releasing 'pOldObject' in the correct order.
void _Object_Assign(ObjectRef _Nullable * _Nonnull pOldObject, ObjectRef _Nullable pNewObject)
{
    ObjectRef pOldObj = *pOldObject;

    if (pOldObj != pNewObject) {
        Object_Release(pOldObj);
        
        if (pNewObject) {
            *pOldObject = Object_Retain(pNewObject);
        } else {
            *pOldObject = NULL;
        }
    }
}

// Assigns 'pNewObject' to the location that holds 'pOldObject' and moves ownership
// of 'pNewObject' from the owner to the location that holds 'pOldObject'. What
// this means is that this function expects that 'pNewObject' is +1, it releases
// 'pOldObject' and stores 'pNewObject' in 'pOldObject'. If both storage locations
// point to the same object then it releases 'pNewObject'. Once this function
// returns 'pOldObject' is properly updated and at +1 while 'pNewObject' is at +0.
void _Object_AssignMovingOwnership(ObjectRef _Nullable * _Nonnull pOldObject, ObjectRef _Nullable pNewObject)
{
    ObjectRef pOldObj = *pOldObject;

    if (pOldObj != pNewObject) {
        // The new object is different from the old one: release the old object
        // and update our object reference to point to the new object (it's
        // already +1)
        Object_Release(pOldObj);
        *pOldObject = pNewObject;
    } else {
        // The old and the new object are the same: no need to do anything with
        // our object reference. However release the new object reference since
        // it's an extra +1
        Object_Release(pNewObject);
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

void _RegisterClass(ClassRef _Nonnull pClass)
{
    if ((pClass->flags & CLASSF_INITIALIZED) != 0) {
        return;
    }

    // Make sure that the super class is registered
    ClassRef pSuperClass = pClass->super;
    if (pSuperClass) {
        _RegisterClass(pSuperClass);
    }


    // Copy the super class vtable
    if (pSuperClass) {
        Bytes_CopyRange(pClass->vtable, pSuperClass->vtable, pSuperClass->methodCount * sizeof(Method));
    }


    // Override methods in the VTable with methods from our method list
    const struct MethodDecl* pCurMethod = pClass->methodList;
    while (pCurMethod->method) {
        Method* pSlot = (Method*)((char*)pClass->vtable + pCurMethod->offset);

        *pSlot = pCurMethod->method;
        pCurMethod++;
    }


    // Make sure that none if the VTable entries is NULL or a bogus pointer
    const int parentMethodCount = (pSuperClass) ? pSuperClass->methodCount : 0;
    for (int i = parentMethodCount; i < pClass->methodCount; i++) {
        if (pClass->vtable[i] == NULL) {
            fatal("RegisterClass: missing %s method at vtable index #%d\n", pClass->name, i);
            // NOT REACHED
        }
    }

    pClass->flags |= CLASSF_INITIALIZED;
}

#ifndef __DISKIMAGE__
// Scans the "__class" data section bounded by the '_class' and '_eclass' linker
// symbols for class records and:
// - builds the vtable for each class
// - validates the vtable
// Must be called after the DATA and BSS segments have been established and before
// and code is invoked that might use objects.
// Note that this function is not concurrency safe.
void RegisterClasses(void)
{
    ClassRef pClass = (ClassRef)&_class;
    ClassRef pEndClass = (ClassRef)&_eclass;

    while (pClass < pEndClass) {
        _RegisterClass(pClass);
        pClass++;
    }
}

// Prints all registered classes
// Note that this function is not concurrency safe.
void PrintClasses(void)
{
    ClassRef pClass = (ClassRef)&_class;
    ClassRef pEndClass = (ClassRef)&_eclass;

    print("_class: %p, _eclass: %p\n", pClass, pEndClass);

    while (pClass < pEndClass) {
        if (pClass->super) {
            print("%s : %s\t\t", pClass->name, pClass->super->name);
        } else {
            print("%s\t\t\t\t", pClass->name);
        }
        print("mcount: %d\tisize: %d\n", pClass->methodCount, pClass->instanceSize);

#if 0
        for (int i = 0; i < pClass->methodCount; i++) {
            print("%d: 0x%p\n", i, pClass->vtable[i]);
        }
#endif
        pClass++;
    }
}
#endif /* __DISKIMAGE__ */
