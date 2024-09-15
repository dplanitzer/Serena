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
#include <kobj/Any.h>


// The Object class is the top type of all reference objects which use a
// standard reference counting memory management model.
open_class(Object, Any,
    AtomicInt   retainCount;
);
any_subclass_funcs(Object,
    // Invoked when the last strong reference of the object has been released.
    // Overrides should release all resources held by the object.
    // Note that you do not need to call the super implementation from your
    // override. The object runtime takes care of that automatically. 
    void    (*deinit)(void* _Nonnull self);
);


// Allocates an instance of the given class. 'extraByteCount' is the number of
// extra bytes that should be allocated for the instance on top of the instance
// size recorded in the class. Returns an error if allocation has failed for
// some reason. The returned object has a reference count of 1. All object
// instance variables are initially set to 0.
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
#define Object_Retain(__self) \
    _Object_Retain((ObjectRef) __self)

#define Object_RetainAs(__self, __classType) \
    ((__classType##Ref) _Object_Retain((ObjectRef) __self))


// Releases a strong reference on the given object. Deallocates the object when
// the reference count transitions from 1 to 0. Invokes the deinit method on
// the object if the object should be deallocated.
#define Object_Release(__self) \
    _Object_Release((ObjectRef) __self)



// Do not call these functions directly. Use the macros defined above instead.
extern errno_t _Object_Create(Class* _Nonnull pClass, size_t extraByteCount, ObjectRef _Nullable * _Nonnull pOutObject);
extern void _Object_Release(ObjectRef _Nullable self);

static inline ObjectRef _Nonnull _Object_Retain(ObjectRef _Nonnull self)
{
    AtomicInt_Increment(&self->retainCount);
    return self;
}

#endif /* Object_h */
