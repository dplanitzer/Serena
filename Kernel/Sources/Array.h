//
//  Array.h
//  Apollo
//
//  Created by Dietmar Planitzer on 3/22/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Array_h
#define Array_h

#include "Foundation.h"


//
// A type generic array
//

typedef struct _Array {
    Byte* _Nonnull  bytes;
    Int             capacity;
    Int             count;
} Array;


// XXX need error code back
#define GenericArray_Init(Type, pArray, minCapacity) \
do { \
    pArray->count = 0;\
    pArray->capacity = max(minCapacity, 0);\
    kalloc(sizeof(Type) * pArray->capacity, (Byte**) &pArray->bytes);\
} while(0)

#define GenericArray_Deinit(Type, pArray) \
do {\
    kfree(pArray->bytes);\
    pArray->bytes = NULL;\
    pArray->capacity = 0;\
    pArray->count = 0;\
} while(0)

// XXX need error code back
#define GenericArray_Create(Type, minCapacity, pOutArray) \
do {\
    if (kalloc(sizeof(Array), (Byte**) pOutArray) == EOK) {\
        GenericArray_Init(Type, *pOutArray, minCapacity);\
    }\
} while(0)

#define GenericArray_Destroy(Type, pArray) \
do {\
    if (pArray) {\
        GenericArray_Deinit(Type, pArray);\
        kfree((Byte*)pArray);\
    }\
} while(0)


#define GenericArray_Count(Type, pArray)\
(pArray->count)

#define GenericArray_IsEmpty(Type, pArray)\
(pArray->count == 0)


#define GenericArray_GetAtUnchecked(Type, pArray, index)\
((Type*)pArray->bytes)[index]

#define GenericArray_SetAtUnchecked(Type, pArray, index, element)\
((Type*)pArray->bytes)[index] = element

#define GenericArray_IndexCheckpoint(Type, pArray, index)\
    assert(index >= 0 && index < pArray->count);


#define GenericArray_Add(Type, pArray, element)\
    GenericArray_InsertAt(Type, pArray, pArray->count, element)


// XXX need error handling
#define GenericArray_InsertAt(Type, pArray, index, element)\
do {\
    assert(index >= 0 && index <= pArray->count); \
    \
    if (pArray->count == pArray->capacity) {\
        const Int newCapacity = pArray->capacity + 1;\
        Type* pOldElements = (Type*)pArray->bytes;\
        Type* pNewElements = NULL;\
        (void)kalloc(sizeof(Type) * newCapacity, (Byte**) &pNewElements);\
        \
        for (Int i = 0; i < index; i++) {\
            pNewElements[i] = pOldElements[i];\
        }\
        \
        pNewElements[index] = element;\
        \
        for (Int i = index; i < pArray->count; i++) {\
            pNewElements[i + 1] = pOldElements[i];\
        }\
        \
        kfree(pArray->bytes);\
        pArray->bytes = (Byte*)pNewElements;\
        pArray->capacity = newCapacity;\
        pArray->count++;\
    } else {\
        Type* pElements = (Type*)pArray->bytes;\
        \
        for(Int i = pArray->count - 1; i >= index; i--) {\
            pElements[i + 1] = pElements[i];\
        }\
        pElements[index] = element;\
        \
        pArray->count++;\
    }\
} while(0)

#define GenericArray_RemoveAt(Type, pArray, index)\
do {\
    assert(index >= 0 && index < pArray->count);\
    Type* pElements = (Type*)pArray->bytes;\
    for(Int i = index + 1; i < pArray->count; i++) {\
        pElements[i - 1] = pElements[i];\
    }\
    pArray->count--;\
} while(0)

#define GenericArray_RemoveAll(Type, pArray, keepCapacity)\
do {\
    pArray->count = 0;\
    if (!keepCapacity) {\
        kfree(pArray->bytes);\
        kalloc(0, (Byte**) &pArray->bytes);\
        pArray->capacity = 0;\
    }\
} while(0)


//
// A specialization of the type generic array which stores pointers to untyped memory
//

extern Array* _Nullable PointerArray_Create(Int minCapacity);
extern void PointerArray_Init(Array* _Nonnull pArray, Int minCapacity);

extern void PointerArray_Deinit(Array* _Nonnull pArray);
extern void PointerArray_Destroy(Array* _Nullable pArray);

static inline Int PointerArray_Count(Array* _Nonnull pArray)
{
    return GenericArray_Count(Byte*, pArray);
}

static inline Bool PointerArray_IsEmpty(Array* _Nonnull pArray)
{
    return GenericArray_IsEmpty(Byte*, pArray);
}

static inline Byte* _Nullable PointerArray_GetAtUnchecked(Array* _Nonnull pArray, Int index)
{
    return GenericArray_GetAtUnchecked(Byte*, pArray, index);
}

static inline void PointerArray_SetAtUnchecked(Array* _Nonnull pArray, Int index, Byte* _Nullable ptr)
{
    GenericArray_SetAtUnchecked(Byte*, pArray, index, ptr);
}

extern Int PointerArray_getIndexOfPointerIdenticalTo(Array* _Nonnull pArray, Byte* _Nullable ptr);

extern void PointerArray_Add(Array* _Nonnull pArray, Byte* _Nullable ptr);
extern void PointerArray_InsertAt(Array* _Nonnull pArray, Int index, Byte* _Nullable ptr);
extern void PointerArray_RemoveAt(Array* _Nonnull pArray, Int index);
extern void PointerArray_RemoveAll(Array* _Nonnull pArray, Bool keepCapacity);

static inline void PointerArray_RemoveIdenticalTo(Array* _Nonnull pArray, Byte* _Nullable ptr)
{
    const Int idx = PointerArray_getIndexOfPointerIdenticalTo(pArray, ptr);
    
    if (idx != -1) {
        PointerArray_RemoveAt(pArray, idx);
    }
}

extern void PointerArray_Dump(Array* _Nonnull pArray);

#endif /* Array_h */
