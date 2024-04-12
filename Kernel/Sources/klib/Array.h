//
//  Array.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Array_h
#define Array_h

#include <klib/Types.h>
#include <klib/Assert.h>
#include <klib/Memory.h>
#include <klib/Error.h>
#include <klib/Kalloc.h>


// A generic array implementation. A generic array stores N elements of type
// 'elementType'. Look for the specializations below if you just want to use an
// array for a specific element type. If you want to create a new specialization
// then you should use the functions/macros here to build your specialization.
struct GenericArray {
    char*   data;
    ssize_t count;
    ssize_t capacity;
};


extern errno_t GenericArray_Init(struct GenericArray* _Nonnull pArray, size_t elementSize, ssize_t initialCapacity);
extern void GenericArray_Deinit(struct GenericArray* _Nonnull pArray);

extern errno_t GenericArray_GrowCapacity(struct GenericArray* _Nonnull pArray, size_t elementSize);

#define GenericArray_GetCount(pArray) (pArray)->count
#define GenericArray_IsEmpty(pArray) ((pArray)->count == 0)

#define GenericArray_GetAt(pArray, elementType, idx) ((elementType*)(pArray)->data)[idx]
#define GenericArray_GetRefAt(pArray, elementType, idx) (&((elementType*)(pArray)->data)[idx])

#define GenericArray_InsertAt(ec, pArray, element, elementType, idx) \
    ec = EOK; \
    if ((pArray)->count == (pArray)->capacity) { \
        ec = GenericArray_GrowCapacity(pArray, sizeof(elementType)); \
    } \
    if (ec == EOK) { \
        elementType* __p = (elementType*)(pArray)->data; \
        for (ssize_t i = (pArray)->count - 1; i >= idx; i--) { \
            __p[i + 1] = __p[i]; \
        } \
        __p[idx] = element; \
        (pArray)->count++; \
    }

#define GenericArray_ReplaceAt(pArray, element, elementType, idx) \
    assert(idx >= 0 && idx < (pArray)->count); \
    ((elementType*)(pArray)->data)[idx] = element

#define GenericArray_RemoveIdenticalTo(didRemove, pArray, element, elementType) \
    { \
        elementType* __p = (elementType*)(pArray)->data; \
        const ssize_t __count = (pArray)->count; \
        didRemove = false; \
        for (ssize_t i = 0; i < __count; i++) { \
            if (__p[i] == element) { \
                i++; \
                for (; i < __count; i++) { \
                    __p[i - 1] = __p[i]; \
                } \
                (pArray)->count--; \
                didRemove = true; \
                break; \
            } \
        } \
    }

#define GenericArray_RemoveAt(oldElement, pArray, elementType, idx) \
    { \
        assert(idx >= 0 && idx < (pArray)->count); \
        elementType* __p = (elementType*)(pArray)->data; \
        oldElement = __p[idx]; \
        for (ssize_t i = idx + 1; i < (pArray)->count; i++) { \
            __p[i - 1] = __p[i]; \
        } \
        (pArray)->count--; \
    }

extern void GenericArray_RemoveAll(struct GenericArray* _Nonnull pArray, bool keepCapacity);

#define GenericArray_FirstIndexOf(idx, pArray, element, elementType) \
    { \
        const elementType* __p = (const elementType*)(pArray)->data; \
        idx = -1; \
        for (ssize_t i = 0; i < pArray->count; i++) { \
            if (__p[i] == element) { \
                idx = i; \
                break; \
            } \
        } \
    }

#define GenericArray_GetFirst(r, pArray, elementType, defaultValue) \
    (r) = ((pArray)->count > 0) ? ((const elementType*) (pArray)->data)[0] : (defaultValue)


// An array that stores int values
typedef struct GenericArray IntArray;
typedef IntArray* IntArrayRef;

#define IntArray_Init(pArray, initialCapacity)  GenericArray_Init((IntArrayRef)(pArray), sizeof(int), initialCapacity)
#define IntArray_Deinit(pArray) GenericArray_Deinit((IntArrayRef)(pArray))

#define IntArray_GetCount(pArray) ((IntArrayRef)(pArray))->count
#define IntArray_IsEmpty(pArray) (((IntArrayRef)(pArray))->count == 0)

#define IntArray_GetAt(pArray, idx) GenericArray_GetAt((IntArrayRef)(pArray), ssize_t, idx)

extern errno_t IntArray_InsertAt(IntArrayRef _Nonnull pArray, int element, ssize_t idx);
#define IntArray_Add(pArray, element) IntArray_InsertAt((IntArrayRef)(pArray), element, (pArray)->count)

#define IntArray_ReplaceAt(pArray, element, idx) GenericArray_ReplaceAt((IntArrayRef)(pArray), element, ssize_t, idx)

extern void IntArray_Remove(IntArrayRef _Nonnull pArray, int element);
extern void IntArray_RemoveAt(IntArrayRef _Nonnull pArray, ssize_t idx);
#define IntArray_RemoveAll(pArray, keepCapacity) GenericArray_RemoveAll((IntArrayRef)(pArray), keepCapacity)

extern bool IntArray_Contains(IntArrayRef _Nonnull pArray, int element);

static inline int IntArray_GetFirst(IntArrayRef _Nonnull pArray, int defaultValue) {
    int r;
    GenericArray_GetFirst(r, pArray, int, defaultValue);
    return r;
}


// An array that stores pointers
typedef struct GenericArray PointerArray;
typedef PointerArray* PointerArrayRef;

#define PointerArray_Init(pArray, initialCapacity)  GenericArray_Init((PointerArrayRef)(pArray), sizeof(void*), initialCapacity)
#define PointerArray_Deinit(pArray) GenericArray_Deinit((PointerArrayRef)(pArray))

#define PointerArray_GetCount(pArray) ((PointerArrayRef)(pArray))->count
#define PointerArray_IsEmpty(pArray) (((PointerArrayRef)(pArray))->count == 0)

#define PointerArray_GetAt(pArray, idx) GenericArray_GetAt((PointerArrayRef)(pArray), void*, idx)
#define PointerArray_GetAtAs(pArray, idx, ty) (ty)(GenericArray_GetAt((PointerArrayRef)(pArray), void*, idx))

extern errno_t PointerArray_InsertAt(PointerArrayRef _Nonnull pArray, void* _Nullable element, ssize_t idx);
#define PointerArray_Add(pArray, element) PointerArray_InsertAt((PointerArrayRef)(pArray), element, (pArray)->count)

#define PointerArray_ReplaceAt(pArray, element, idx) GenericArray_ReplaceAt((PointerArrayRef)(pArray), element, void*, idx)

extern void PointerArray_Remove(PointerArrayRef _Nonnull pArray, void* _Nullable element);
extern void PointerArray_RemoveAt(PointerArrayRef _Nonnull pArray, ssize_t idx);
#define PointerArray_RemoveAll(pArray, keepCapacity) GenericArray_RemoveAll((PointerArrayRef)(pArray), keepCapacity)

#endif /* Array_h */
