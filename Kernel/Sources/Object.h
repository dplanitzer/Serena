//
//  Object.h
//  Apollo
//
//  Created by Dietmar Planitzer on 10/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Object_h
#define Object_h

#include <klib/klib.h>

#define Func0(__rtype, __name) \
    __rtype (*__name)(void* self)

#define FuncN(__rtype, __name, ...) \
    __rtype (*__name)(void* self, __VA_ARGS__)


struct _Object;
typedef struct _Object* ObjectRef;

// Deallocate whatever internal resources the object is holding.
typedef Func0(void, Func_Object_Deinit);

typedef struct _Class {
    Func_Object_Deinit _Nullable    deinit;
} Class;

typedef struct _Class* ClassRef;


typedef struct _Object {
    ClassRef _Nonnull   class;
    AtomicInt           retainCount;
} Object;


extern ErrorCode _Object_Create(ClassRef _Nonnull pClass, ByteCount instanceSize, ObjectRef _Nullable * _Nonnull pOutObject);

#define Object_Create(__pClass, __instanceSize, __pOutObject) \
    _Object_Create((ClassRef)__pClass, __instanceSize, (ObjectRef*)__pOutObject)

// Reference counting model for objects:
//
// 1. An object starts out its lifetime with a reference count of 1.
// 2. Use Retain() to increment the reference count to keep an object alive.
// 3. Use Release() to decrement the reference count of an object. The object
//    is deallocated when the reference count reaches 0.

// Retains the given object and returns a (new) strong reference to the given
// object. Retaining an object keeps it alive.
static inline ObjectRef _Nonnull _Object_Retain(ObjectRef _Nonnull self)
{
    AtomicInt_Increment(&self->retainCount);
    return self;
}

#define Object_Retain(__self) \
    _Object_Retain((ObjectRef) __self)

#define Object_RetainAs(__self, __classType) \
    ((__classType##Ref) _Object_Retain((ObjectRef) __self))

// Releases a strong reference on the given object. Deallocates the object when
// the reference count transitions from 1 to 0. Invokes the deinit method on
// the object if the object should be deallocated.
extern void _Object_Release(ObjectRef _Nullable self);

#define Object_Release(__self) \
    _Object_Release((ObjectRef) __self)

// Returns the class of the object cast to the given static class type. Does not
// check whether the dynamic class type is actually the same as the provided
// static class type.
#define Object_GetClassAs(__self, __classType) \
    ((__classType##ClassRef) ((ObjectRef)__self)->class)

// Checks whether the receiver of the given static class type implements a method
// with the given name.
#define Object_Implements(__self, __classType, __funcName) \
    (Object_GetClassAs(__self, __classType)->__funcName != NULL)


////////////////////////////////////////////////////////////////////////////////

struct _UObject;
typedef struct _UObject* UObjectRef;


typedef FuncN(ByteCount, Func_UObject_Read, Byte* _Nonnull pBuffer, ByteCount nBytesToRead);
typedef FuncN(ByteCount, Func_UObject_Write, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite);

// Close the resource. The purpose of the close operation is:
// - flush all data that was written and is still buffered/cached to the underlying device
// - if a write operation is ongoing at the time of the close then let this write operation finish and sync the underlying device
// - if a read operation is ongoing at the time of the close then interrupt the read with an EINTR error
// The resource should be internally marked as closed and all future read/write/etc operations on the resource should do nothing
// and instead return a suitable status. Eg a write should return EIO and a read should return EOF.
// It is permissible for a close operation to block the caller for some (reasonable) amount of time to complete the flush.
// The close operation may return an error. Returning an error will not stop the kernel from completing the close and eventually
// deallocating the resource. The error is passed on to the caller but is purely advisory in nature. The close operation is
// required to mark the resource as closed whether the close internally succeeded or failed. 
typedef Func0(ErrorCode, Func_UObject_Close);

typedef struct _UObjectClass {
    Class                           super;
    Func_UObject_Read _Nullable     read;
    Func_UObject_Write _Nullable    write;
    Func_UObject_Close _Nullable    close;
} UObjectClass;

typedef struct _UObjectClass* UObjectClassRef;

typedef Object UObject;

#define UObject_Close(__pRes) \
    Object_GetClassAs(__pRes, UObject)->close(__pRes)

#define UObject_Read(__pRes, __pBuffer, __nBytesToRead) \
    Object_GetClassAs(__pRes, UObject)->read(__pRes, __pBuffer, __nBytesToRead)

#define UObject_Write(__pRes, __pBuffer, __nBytesToWrite) \
    Object_GetClassAs(__pRes, UObject)->write(__pRes, __pBuffer, __nBytesToWrite)
    
#endif /* Object_h */
