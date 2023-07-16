//
//  Array.c
//  Apollo
//
//  Created by Dietmar Planitzer on 3/22/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Array.h"
#include "Heap.h"
#include "Bytes.h"


Array* _Nullable PointerArray_Create(Int minCapacity)
{
    Array* pArray = NULL;
    
    GenericArray_Create(Byte*, minCapacity, &pArray);
    return pArray;
}

void PointerArray_Init(Array* _Nonnull pArray, Int minCapacity)
{
    GenericArray_Init(Byte*, pArray, minCapacity);
}

void PointerArray_Deinit(Array* _Nonnull pArray)
{
    GenericArray_Deinit(Byte*, pArray);
}

void PointerArray_Destroy(Array* _Nullable pArray)
{
    GenericArray_Destroy(Byte*, pArray);
}

void PointerArray_Add(Array* _Nonnull pArray, Byte* _Nullable ptr)
{
    GenericArray_Add(Byte*, pArray, ptr);
}

void PointerArray_InsertAt(Array* _Nonnull pArray, Int index, Byte* _Nullable ptr)
{
    GenericArray_InsertAt(Byte*, pArray, index, ptr);
}

void PointerArray_RemoveAt(Array* _Nonnull pArray, Int index)
{
    GenericArray_RemoveAt(Byte*, pArray, index);
}

void PointerArray_RemoveAll(Array* _Nonnull pArray, Bool keepCapacity)
{
    GenericArray_RemoveAll(Byte*, pArray, keepCapacity);
}

Int PointerArray_getIndexOfPointerIdenticalTo(Array* _Nonnull pArray, Byte* _Nullable ptr)
{
    for (Int i = 0; i < pArray->count; i++) {
        if (PointerArray_GetAtUnchecked(pArray, i) == ptr) {
            return i;
        }
    }
    
    return -1;
}

void PointerArray_Dump(Array* _Nonnull pArray)
{
    print("Ptr[%lld:%lld] = {", pArray->count, pArray->capacity);
    for (Int i = 0; i < pArray->count; i++) {
        print("%p", PointerArray_GetAtUnchecked(pArray, i));
        if (i < (pArray->count - 1)) {
            print(", ");
        }
    }
    print("};\n");
}
