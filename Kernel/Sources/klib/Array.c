//
//  Array.c
//  Apollo
//
//  Created by Dietmar Planitzer on 10/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Array.h"
#include "Bytes.h"

ErrorCode GenericArray_Init(struct _GenericArray* _Nonnull pArray, Int elementSize, Int initialCapacity)
{
    const ErrorCode err = kalloc(elementSize * initialCapacity, &pArray->data);
    
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

ErrorCode GenericArray_GrowCapacity(struct _GenericArray* _Nonnull pArray, Int elementSize)
{
    const Int newCapacity = (pArray->capacity > 0) ? pArray->capacity * 2 : 8;
    Byte* pNewData;
    const ErrorCode err = kalloc(elementSize * newCapacity, &pNewData);

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

void GenericArray_RemoveAll(struct _GenericArray* _Nonnull pArray, Bool keepCapacity)
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
// MARK: Array<Int>
////////////////////////////////////////////////////////////////////////////////

ErrorCode IntArray_InsertAt(IntArrayRef _Nonnull pArray, Int element, Int idx)
{
    decl_try_err();

    GenericArray_InsertAt(err, pArray, element, Int, idx);
    return err;
}

void IntArray_RemoveAt(IntArrayRef _Nonnull pArray, Int idx)
{
    Int dummy;
    GenericArray_RemoveAt(dummy, pArray, Int, idx);
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

ObjectRef _Nullable ObjectArray_CopyAt(ObjectArrayRef _Nonnull pArray, Int idx)
{
    ObjectRef element = GenericArray_GetAt((ObjectArrayRef)pArray, ObjectRef, idx);
    
    if (element) {
        Object_Retain(element);
    }
    return element;
}

ErrorCode ObjectArray_InsertAt(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element, Int idx)
{
    decl_try_err();

    GenericArray_InsertAt(err, pArray, element, ObjectRef, idx);
    if (err == EOK && element) {
        Object_Retain(element);
    }
    return err;
}

void ObjectArray_ReplaceAt(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element, Int idx)
{
    assert(idx >= 0 && idx < pArray->count);

    ObjectRef* __p = (ObjectRef*)pArray->data;
    if (__p[idx] != element) {
        Object_Release(__p[idx]);
        __p[idx] = (element) ? Object_Retain(element) : element;
    }
}

void ObjectArray_RemoveAt(ObjectArrayRef _Nonnull pArray, Int idx)
{
    ObjectRef oldElement;

    GenericArray_RemoveAt(oldElement, pArray, ObjectRef, idx);
    Object_Release(oldElement);
}

void ObjectArray_RemoveAll(ObjectArrayRef _Nonnull pArray, Bool keepCapacity)
{
    for (Int i = 0; i < pArray->count; i++) {
        Object_Release(((ObjectRef*)pArray->data)[i]);
    }
    GenericArray_RemoveAll(pArray, keepCapacity);
}
