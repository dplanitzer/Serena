//
//  Array.h
//  Apollo
//
//  Created by Dietmar Planitzer on 10/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Array_h
#define Array_h

#include <klib/Types.h>
#include <klib/Assert.h>
#include <klib/Bytes.h>
#include <klib/Error.h>
#include <klib/Kalloc.h>
#include <klib/Object.h>

// A generic array implementation. A generic array stores N elements of type
// 'elementType'. Look for the specializations below if you just want to use an
// array for a specific element type. If you want to create a new specialization
// then you should use the functions/macros here to build your specialization.
struct _GenericArray {
    Byte*   data;
    Int     count;
    Int     capacity;
};


extern ErrorCode GenericArray_Init(struct _GenericArray* _Nonnull pArray, Int elementSize, Int initialCapacity);
extern void GenericArray_Deinit(struct _GenericArray* _Nonnull pArray);

extern ErrorCode GenericArray_GrowCapacity(struct _GenericArray* _Nonnull pArray, Int elementSize);

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
        for (Int i = (pArray)->count - 1; i >= idx; i--) { \
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
        const Int __count = (pArray)->count; \
        for (Int i = 0; i < __count; i++) { \
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
        didRemove = false; \
    }

#define GenericArray_RemoveAt(oldElement, pArray, elementType, idx) \
    { \
        assert(idx >= 0 && idx < (pArray)->count); \
        elementType* __p = (elementType*)(pArray)->data; \
        oldElement = __p[idx]; \
        for (Int i = idx + 1; i < (pArray)->count; i++) { \
            __p[i - 1] = __p[i]; \
        } \
        (pArray)->count--; \
    }

extern void GenericArray_RemoveAll(struct _GenericArray* _Nonnull pArray, Bool keepCapacity);

#define GenericArray_FirstIndexOf(idx, pArray, element, elementType) \
    { \
        const elementType* __p = (const elementType*)(pArray)->data; \
        for (Int i = 0; i < pArray->count; i++) { \
            if (__p[i] == element) { \
                idx = i; \
                break; \
            } \
        } \
        idx = -1; \
    }

#define GenericArray_GetFirst(r, pArray, elementType, defaultValue) \
    (r) = ((pArray)->count > 0) ? ((const elementType*) (pArray)->data)[0] : (defaultValue)


// An array that stores Int values
typedef struct _GenericArray IntArray;
typedef IntArray* IntArrayRef;

#define IntArray_Init(pArray, initialCapacity)  GenericArray_Init((IntArrayRef)(pArray), sizeof(Int), initialCapacity)
#define IntArray_Deinit(pArray) GenericArray_Deinit((IntArrayRef)(pArray))

#define IntArray_GetCount(pArray) ((IntArrayRef)(pArray))->count
#define IntArray_IsEmpty(pArray) (((IntArrayRef)(pArray))->count == 0)

#define IntArray_GetAt(pArray, idx) GenericArray_GetAt((IntArrayRef)(pArray), Int, idx)

extern ErrorCode IntArray_InsertAt(IntArrayRef _Nonnull pArray, Int element, Int idx);
#define IntArray_Add(pArray, element) IntArray_InsertAt((IntArrayRef)(pArray), element, (pArray)->count)

#define IntArray_ReplaceAt(pArray, element, idx) GenericArray_ReplaceAt((IntArrayRef)(pArray), element, Int, idx)

extern void IntArray_Remove(IntArrayRef _Nonnull pArray, Int element);
extern void IntArray_RemoveAt(IntArrayRef _Nonnull pArray, Int idx);
#define IntArray_RemoveAll(pArray, keepCapacity) GenericArray_RemoveAll((IntArrayRef)(pArray), keepCapacity)

extern Bool IntArray_Contains(IntArrayRef _Nonnull pArray, Int element);

static inline Int IntArray_GetFirst(IntArrayRef _Nonnull pArray, Int defaultValue) {
    Int r;
    GenericArray_GetFirst(r, pArray, Int, defaultValue);
    return r;
}


// An array that stores pointers
typedef struct _GenericArray PointerArray;
typedef PointerArray* PointerArrayRef;

#define PointerArray_Init(pArray, initialCapacity)  GenericArray_Init((PointerArrayRef)(pArray), sizeof(void*), initialCapacity)
#define PointerArray_Deinit(pArray) GenericArray_Deinit((PointerArrayRef)(pArray))

#define PointerArray_GetCount(pArray) ((PointerArrayRef)(pArray))->count
#define PointerArray_IsEmpty(pArray) (((PointerArrayRef)(pArray))->count == 0)

#define PointerArray_GetAt(pArray, idx) GenericArray_GetAt((PointerArrayRef)(pArray), void*, idx)
#define PointerArray_GetAtAs(pArray, idx, ty) (ty)(GenericArray_GetAt((PointerArrayRef)(pArray), void*, idx))

extern ErrorCode PointerArray_InsertAt(PointerArrayRef _Nonnull pArray, void* _Nullable element, Int idx);
#define PointerArray_Add(pArray, element) PointerArray_InsertAt((PointerArrayRef)(pArray), element, (pArray)->count)

#define PointerArray_ReplaceAt(pArray, element, idx) GenericArray_ReplaceAt((PointerArrayRef)(pArray), element, void*, idx)

extern void PointerArray_Remove(PointerArrayRef _Nonnull pArray, void* _Nullable element);
extern void PointerArray_RemoveAt(PointerArrayRef _Nonnull pArray, Int idx);
#define PointerArray_RemoveAll(pArray, keepCapacity) GenericArray_RemoveAll((PointerArrayRef)(pArray), keepCapacity)


// An array that stores nullable ObjectRef values
typedef struct _GenericArray ObjectArray;
typedef ObjectArray* ObjectArrayRef;

#define ObjectArray_Init(pArray, initialCapacity)  GenericArray_Init((ObjectArrayRef)(pArray), sizeof(ObjectRef), initialCapacity)
extern void ObjectArray_Deinit(ObjectArrayRef _Nonnull pArray);

#define ObjectArray_GetCount(pArray) ((ObjectArrayRef)(pArray))->count
#define ObjectArray_IsEmpty(pArray) (((ObjectArrayRef)(pArray))->count == 0)

#define ObjectArray_GetAt(pArray, idx) GenericArray_GetAt((ObjectArrayRef)(pArray), ObjectRef, idx)
extern ObjectRef _Nullable ObjectArray_CopyAt(ObjectArrayRef _Nonnull pArray, Int idx);

extern ErrorCode ObjectArray_InsertAt(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element, Int idx);
#define ObjectArray_Add(pArray, element) ObjectArray_InsertAt((ObjectArrayRef)(pArray), element, (pArray)->count)

extern void ObjectArray_ReplaceAt(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element, Int idx);

extern void ObjectArray_RemoveIdenticalTo(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element);
extern void ObjectArray_RemoveAt(ObjectArrayRef _Nonnull pArray, Int idx);
extern void ObjectArray_RemoveAll(ObjectArrayRef _Nonnull pArray, Bool keepCapacity);

// Returns the original object and sets the entry it occupied to NULL. Note that
// the caller is responsible for releasing the object. This function does not
// release it. It transfers ownership to the caller.
extern ObjectRef _Nullable ObjectArray_ExtractOwnershipAt(ObjectArrayRef _Nonnull pArray, Int idx);

#endif /* Array_h */
