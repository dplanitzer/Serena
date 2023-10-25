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


#ifdef __section
#define _ClassSection __section("__class")
#else
#define _ClassSection
#endif

#define CLASS_FORWARD(__name) \
struct _##__name; \
typedef struct _##__name* __name##Ref


#define INSTANCE_METHOD_IMPL(__name, __className) \
{ k##__className##MethodIndex_##__name , (Method) __className##_##__name },

#define OVERRIDE_METHOD_IMPL(__name, __className, __superClassName) \
{ k##__superClassName##MethodIndex_##__name , (Method) __className##_##__name },


#define __CLASS_IMPLEMENTATION(__name, __super, ...) \
static Method g##__name##VTable[k##__name##MethodIndex_Count];\
static const struct MethodDecl gMethodImpls_##__name[] = { __VA_ARGS__ {0, (Method)0} }; \
Class _ClassSection k##__name##Class = {g##__name##VTable, __super, #__name, sizeof(__name), k##__name##MethodIndex_Count, 0, gMethodImpls_##__name}

#define __ROOT_CLASS_INTERFACE(__name, __super, __ivars_decls) \
extern Class k##__name##Class; \
typedef struct _##__name { __super __ivars_decls } __name


#define ROOT_CLASS_IMPLEMENTATION(__name, ...) __CLASS_IMPLEMENTATION(__name, NULL, __VA_ARGS__)
#define ROOT_CLASS_INTERFACE(__name, __ivar_decls) __ROOT_CLASS_INTERFACE(__name, , __ivar_decls)


#define CLASS_IMPLEMENTATION(__name, __super, ...) __CLASS_IMPLEMENTATION(__name, &k##__super##Class, __VA_ARGS__)
#define CLASS_INTERFACE(__name, __super, __ivar_decls) __ROOT_CLASS_INTERFACE(__name, __super super;, __ivar_decls)


#define INSTANCE_METHOD_0(__rtype, __className, __methodName) \
typedef __rtype (*__className##Method_##__methodName)(void* self)

#define INSTANCE_METHOD_N(__rtype, __className, __methodName, ...) \
typedef __rtype (*__className##Method_##__methodName)(void* self, __VA_ARGS__)


typedef void (*Method)(void* self, ...);

struct MethodDecl {
    Int             index;
    Method _Nonnull method;
};

#define CLASSF_INITIALIZED  1

typedef struct __Class {
    Method* _Nonnull            vtable;
    struct __Class* _Nonnull    super;
    const char* _Nonnull        name;
    ByteCount                   instanceSize;
    Int16                       methodCount;
    UInt16                      flags;
    const struct MethodDecl* _Nonnull methodList;
} Class;
typedef Class* ClassRef;


CLASS_FORWARD(Object);

ROOT_CLASS_INTERFACE(Object,
    ClassRef _Nonnull   class;
    AtomicInt           retainCount;
);
enum ObjectMethodIndex {
    kObjectMethodIndex_deinit,
    
    kObjectMethodIndex_Count = kObjectMethodIndex_deinit + 1
};
INSTANCE_METHOD_0(void, Object, deinit);

extern ErrorCode _Object_Create(ClassRef _Nonnull pClass, ByteCount extraByteCount, ObjectRef _Nullable * _Nonnull pOutObject);

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
((Class*)(((ObjectRef)(__self))->class))

#define Object_InstanceOf(__self, __className) \
    (Object_GetClass(__self) == &k##__className##Class)

#define Object_Invoke(__methodName, __methodClassName, __self, ...) \
((__methodClassName##Method_##__methodName)(Object_GetClass(__self)->vtable[k##__methodClassName##MethodIndex_##__methodName]))(__self, __VA_ARGS__)

// Scans the "__class" data section bounded by the '_class' and '_eclass' linker
// symbols for class records and:
// - builds the vtable for each class
// - validates the vtable
// Must be called after the DATA and BSS segments have been established and before
// and code is invoked that might use objects.
// Note that this function is not concurrency safe.
extern void RegisterClasses(void);

// Prints all registered classes
// Note that this function is not concurrency safe.
extern void PrintClasses(void);

#endif /* Object_h */
