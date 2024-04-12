//
//  ObjectArray.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ObjectArray_h
#define ObjectArray_h

#include <klib/Array.h>
#include <kobj/Object.h>


//
// Array<Object?>
//

typedef struct GenericArray ObjectArray;
typedef ObjectArray* ObjectArrayRef;

#define ObjectArray_Init(pArray, initialCapacity)  GenericArray_Init((ObjectArrayRef)(pArray), sizeof(ObjectRef), initialCapacity)
extern void ObjectArray_Deinit(ObjectArrayRef _Nonnull pArray);

#define ObjectArray_GetCount(pArray) ((ObjectArrayRef)(pArray))->count
#define ObjectArray_IsEmpty(pArray) (((ObjectArrayRef)(pArray))->count == 0)

#define ObjectArray_GetAt(pArray, idx) GenericArray_GetAt((ObjectArrayRef)(pArray), ObjectRef, idx)
extern ObjectRef _Nullable ObjectArray_CopyAt(ObjectArrayRef _Nonnull pArray, ssize_t idx);

extern errno_t ObjectArray_InsertAt(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element, ssize_t idx);
#define ObjectArray_Add(pArray, element) ObjectArray_InsertAt((ObjectArrayRef)(pArray), element, (pArray)->count)

extern void ObjectArray_ReplaceAt(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element, ssize_t idx);

extern void ObjectArray_RemoveIdenticalTo(ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable element);
extern void ObjectArray_RemoveAt(ObjectArrayRef _Nonnull pArray, ssize_t idx);
extern void ObjectArray_RemoveAll(ObjectArrayRef _Nonnull pArray, bool keepCapacity);

// Returns the original object and sets the entry it occupied to NULL. Note that
// the caller is responsible for releasing the object. This function does not
// release it. It transfers ownership to the caller.
extern ObjectRef _Nullable ObjectArray_ExtractOwnershipAt(ObjectArrayRef _Nonnull pArray, ssize_t idx);

#endif /* ObjectArray_h */
