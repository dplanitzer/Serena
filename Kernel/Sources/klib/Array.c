//
//  Array.c
//  Apollo
//
//  Created by Dietmar Planitzer on 10/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Array.h"
#include "Bytes.h"

errno_t GenericArray_Init(struct _GenericArray* _Nonnull pArray, size_t elementSize, ssize_t initialCapacity)
{
    const errno_t err = kalloc(elementSize * initialCapacity, (void**)&pArray->data);
    
    if (err == EOK) {
        pArray->count = 0;
        pArray->capacity = initialCapacity;
    } else {
        pArray->data = NULL;
        pArray->count = 0;
        pArray->capacity = 0;
    }
    return err;
}

void GenericArray_Deinit(struct _GenericArray* pArray)
{
    kfree(pArray->data);
    pArray->data = NULL;
}

errno_t GenericArray_GrowCapacity(struct _GenericArray* _Nonnull pArray, size_t elementSize)
{
    const ssize_t newCapacity = (pArray->capacity > 0) ? pArray->capacity * 2 : 8;
    char* pNewData;
    const errno_t err = kalloc(elementSize * newCapacity, (void**)&pNewData);

    if (err == EOK) {
        if (pArray->count > 0) {
            Bytes_CopyRange(pNewData, pArray->data, pArray->count * elementSize);
        }
        kfree(pArray->data);
        pArray->data = pNewData;
        pArray->capacity = newCapacity;
    }
    return err;
}

void GenericArray_RemoveAll(struct _GenericArray* _Nonnull pArray, bool keepCapacity)
{
    pArray->count = 0;
    if (!keepCapacity) {
        kfree(pArray->data);
        pArray->data = NULL;
        pArray->capacity = 0;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Array<int>
////////////////////////////////////////////////////////////////////////////////

errno_t IntArray_InsertAt(IntArrayRef _Nonnull pArray, int element, ssize_t idx)
{
    decl_try_err();

    GenericArray_InsertAt(err, pArray, element, int, idx);
    return err;
}

void IntArray_Remove(IntArrayRef _Nonnull pArray, int element)
{
    bool dummy;
    GenericArray_RemoveIdenticalTo(dummy, pArray, element, int);
}

void IntArray_RemoveAt(IntArrayRef _Nonnull pArray, ssize_t idx)
{
    int dummy;
    GenericArray_RemoveAt(dummy, pArray, int, idx);
}

bool IntArray_Contains(IntArrayRef _Nonnull pArray, int element)
{
    ssize_t idx;

    GenericArray_FirstIndexOf(idx, pArray, element, int);
    return idx != -1;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Array<Pointer?>
////////////////////////////////////////////////////////////////////////////////

errno_t PointerArray_InsertAt(PointerArrayRef _Nonnull pArray, void* _Nullable element, ssize_t idx)
{
    decl_try_err();

    GenericArray_InsertAt(err, pArray, element, void*, idx);
    return err;
}

void PointerArray_Remove(PointerArrayRef _Nonnull pArray, void* _Nullable element)
{
    bool dummy;
    GenericArray_RemoveIdenticalTo(dummy, pArray, element, void*);
}

void PointerArray_RemoveAt(PointerArrayRef _Nonnull pArray, ssize_t idx)
{
    void* dummy;
    GenericArray_RemoveAt(dummy, pArray, void*, idx);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Array<Object?>
////////////////////////////////////////////////////////////////////////////////

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

