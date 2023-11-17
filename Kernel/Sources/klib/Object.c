//
//  Object.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Object.h"
#include "Bytes.h"
#include "Kalloc.h"
#include "Log.h"

extern char _class;
extern char _eclass;


void Object_deinit(ObjectRef _Nonnull self)
{
}

ROOT_CLASS_METHODS(Object,
METHOD_IMPL(deinit, Object)
);

ErrorCode _Object_Create(ClassRef _Nonnull pClass, ByteCount extraByteCount, ObjectRef _Nullable * _Nonnull pOutObject)
{
    decl_try_err();
    ObjectRef pObject;

    if (extraByteCount > 0) {
        extraByteCount--;   // Account for the byte defined in the IOChannel structure
    }

    try(kalloc_cleared(pClass->instanceSize + extraByteCount, (Byte**) &pObject));
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
        kfree((Byte*) self);
    }
}

Bool _Object_InstanceOf(ObjectRef _Nonnull pObj, ClassRef _Nonnull pTargetClass)
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

static void RegisterClass(ClassRef _Nonnull pClass)
{
    if ((pClass->flags & CLASSF_INITIALIZED) != 0) {
        return;
    }

    // Make sure that the super class is registered
    ClassRef pSuperClass = pClass->super;
    if (pSuperClass) {
        RegisterClass(pSuperClass);
    }


    // Copy the super class vtable
    if (pSuperClass) {
        Bytes_CopyRange((Byte*)pClass->vtable, (const Byte*)pSuperClass->vtable, pSuperClass->methodCount * sizeof(Method));
    }


    // Override methods in the VTable with methods from our method list
    const struct MethodDecl* pCurMethod = pClass->methodList;
    while (pCurMethod->method) {
        Method* pSlot = (Method*)((Byte*)pClass->vtable + pCurMethod->offset);

        *pSlot = pCurMethod->method;
        pCurMethod++;
    }


    // Make sure that none if the VTable entries is NULL or a bogus pointer
    const Int parentMethodCount = (pSuperClass) ? pSuperClass->methodCount : 0;
    for (Int i = parentMethodCount; i < pClass->methodCount; i++) {
        if (pClass->vtable[i] == NULL) {
            fatal("RegisterClass: missing %s method at vtable index #%d\n", pClass->name, i);
            // NOT REACHED
        }
    }

    pClass->flags |= CLASSF_INITIALIZED;
}

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
        RegisterClass(pClass);
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
        for (Int i = 0; i < pClass->methodCount; i++) {
            print("%d: 0x%p\n", i, pClass->vtable[i]);
        }
#endif
        pClass++;
    }
}
