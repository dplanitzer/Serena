//
//  Array.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Array.h"
#include "Memory.h"

errno_t GenericArray_Init(struct GenericArray* _Nonnull pArray, size_t elementSize, ssize_t initialCapacity)
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

void GenericArray_Deinit(struct GenericArray* pArray)
{
    kfree(pArray->data);
    pArray->data = NULL;
}

errno_t GenericArray_GrowCapacity(struct GenericArray* _Nonnull pArray, size_t elementSize)
{
    const ssize_t newCapacity = (pArray->capacity > 0) ? pArray->capacity * 2 : 8;
    char* pNewData;
    const errno_t err = kalloc(elementSize * newCapacity, (void**)&pNewData);

    if (err == EOK) {
        if (pArray->count > 0) {
            memcpy(pNewData, pArray->data, pArray->count * elementSize);
        }
        kfree(pArray->data);
        pArray->data = pNewData;
        pArray->capacity = newCapacity;
    }
    return err;
}

void GenericArray_RemoveAll(struct GenericArray* _Nonnull pArray, bool keepCapacity)
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
