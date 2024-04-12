//
//  ObjectArray.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ObjectArray.h"
#include <klib/Memory.h>


void ObjectArray_Deinit(ObjectArrayRef _Nonnull pArray)
{
    ObjectArray_RemoveAll(pArray, true);
    GenericArray_Deinit(pArray);
}

ObjectRef _Nullable ObjectArray_CopyAt(ObjectArrayRef _Nonnull pArray, ssize_t idx)
{
    ObjectRef element = GenericArray_GetAt((ObjectArrayRef)pArray, ObjectRef, idx);
    
    if (element) {
        Object_Retain(element);
    }
    return element;
}

errno_t ObjectArray_InsertAt(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element, ssize_t idx)
{
    decl_try_err();

    GenericArray_InsertAt(err, pArray, element, ObjectRef, idx);
    if (err == EOK && element) {
        Object_Retain(element);
    }
    return err;
}

void ObjectArray_ReplaceAt(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element, ssize_t idx)
{
    assert(idx >= 0 && idx < pArray->count);

    ObjectRef* __p = (ObjectRef*)pArray->data;
    if (__p[idx] != element) {
        Object_Release(__p[idx]);
        __p[idx] = (element) ? Object_Retain(element) : element;
    }
}

void ObjectArray_RemoveIdenticalTo(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element)
{
    bool didRemove;

    GenericArray_RemoveIdenticalTo(didRemove, pArray, element, ObjectRef);
    if (didRemove) {
        Object_Release(element);
    }
}

void ObjectArray_RemoveAt(ObjectArrayRef _Nonnull pArray, ssize_t idx)
{
    ObjectRef oldElement;

    GenericArray_RemoveAt(oldElement, pArray, ObjectRef, idx);
    Object_Release(oldElement);
}

void ObjectArray_RemoveAll(ObjectArrayRef _Nonnull pArray, bool keepCapacity)
{
    for (ssize_t i = 0; i < pArray->count; i++) {
        Object_Release(((ObjectRef*)pArray->data)[i]);
    }
    GenericArray_RemoveAll(pArray, keepCapacity);
}

ObjectRef _Nullable ObjectArray_ExtractOwnershipAt(ObjectArrayRef _Nonnull pArray, ssize_t idx)
{
    assert(idx >= 0 && idx < pArray->count);

    ObjectRef* __p = (ObjectRef*)pArray->data;
    ObjectRef element = __p[idx];
    __p[idx] = NULL;
    return element;
}

