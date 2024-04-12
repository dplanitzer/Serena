//
//  Object.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Object_h
#define Object_h

#include <klib/Atomic.h>
#include <klib/Error.h>
#include <kobj/Runtime.h>


// The Object class. This is the root class aka top type of the object system.
// All other classes derive directly or indirectly from Object. The only
// operations supported by Object are reference counting based memory management
// and dynamic method invocation.
// Dynamic method invocation is implemented via index-based method dispatching.
// Every method is assigned an index and this index is used to look up the
// implementation of a method.
root_class_with_ref(Object,
    ClassRef _Nonnull   clazz;
    AtomicInt           retainCount;
);
typedef struct ObjectMethodTable {
    void    (*deinit)(void* _Nonnull self);
} ObjectMethodTable;


// Allocates an instance of the given class. 'extraByteCount' is the number of
// extra bytes that should be allocated for the instance on top of the instance
// size recorded in the class. Returns an error if allocation has failed for
// some reason. The returned object has a reference count of 1. All object
// instance variables are initially set to 0.
extern errno_t _Object_Create(ClassRef _Nonnull pClass, size_t extraByteCount, ObjectRef _Nullable * _Nonnull pOutObject);

#define Object_Create(__className, __pOutObject) \
    _Object_Create(&k##__className##Class, 0, (ObjectRef*)__pOutObject)

#define Object_CreateWithExtraBytes(__className, __extraByteCount, __pOutObject) \
    _Object_Create(&k##__className##Class, __extraByteCount, (ObjectRef*)__pOutObject)


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


#define Object_GetClass(__self)\
    ((Class*)(((ObjectRef)(__self))->clazz))

#define Object_GetSuperclass(__self)\
    Object_GetClass(__self)->super


// Returns true if the given object is an instance of the given class or one of
// the super classes.
extern bool _Object_InstanceOf(ObjectRef _Nonnull pObj, ClassRef _Nonnull pTargetClass);

#define Object_InstanceOf(__self, __className) \
    _Object_InstanceOf((ObjectRef)__self, &k##__className##Class)


// Invokes a dynamically dispatched method with the given arguments.
#define Object_Invoke0(__methodName, __methodClassName, __self) \
    ((struct __methodClassName##MethodTable *)(Object_GetClass(__self)->vtable))->__methodName(__self)

#define Object_InvokeN(__methodName, __methodClassName, __self, ...) \
    ((struct __methodClassName##MethodTable *)(Object_GetClass(__self)->vtable))->__methodName(__self, __VA_ARGS__)


// Invokes a dynamically dispatched method with the given arguments on the superclass of the receiver.
#define Object_Super0(__methodName, __methodClassName, __self) \
    ((struct __methodClassName##MethodTable *)(Object_GetSuperclass(__self)->vtable))->__methodName(__self)

#define Object_SuperN(__methodName, __methodClassName, __self, ...) \
    ((struct __methodClassName##MethodTable *)(Object_GetSuperclass(__self)->vtable))->__methodName(__self, __VA_ARGS__)

#endif /* Object_h */
