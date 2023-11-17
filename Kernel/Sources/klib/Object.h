//
//  Object.h
//  Apollo
//
//  Created by Dietmar Planitzer on 10/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Object_h
#define Object_h

#include <klib/Atomic.h>
#include <klib/Error.h>
#include <klib/Types.h>


// This section stores all class data structures. The class data structures are
// defined by the macros below.
#ifdef __VBCC__
#define _ClassSection __section("__class")
#else
#define _ClassSection
#endif


// A method implementation declaration. This adds the method named '__name' of
// the class named '__className' to this class. Use this macro to declare a
// method that does not override the equally named superclass method.
#define METHOD_IMPL(__name, __className) \
{ (Method) __className##_##__name, offsetof(struct _##__className##MethodTable, __name) },

// Same as METHOD_IMPL except that it declares the method '__name' to be an override
// of the method with the same name and type signature which was originally defined
// in the class '__superClassName'.
#define OVERRIDE_METHOD_IMPL(__name, __className, __superClassName) \
{ (Method) __className##_##__name, offsetof(struct _##__superClassName##MethodTable, __name) },


#define __CLASS_METHODS(__name, __super, ...) \
static struct _##__name##MethodTable g##__name##VTable; \
static const struct MethodDecl gMethodImpls_##__name[] = { __VA_ARGS__ {(Method)0, 0} }; \
Class _ClassSection k##__name##Class = {(Method*)&g##__name##VTable, __super, #__name, sizeof(__name), (Int16) sizeof(struct _##__name##MethodTable)/sizeof(Method), 0, gMethodImpls_##__name}

#define __CLASS_IVARS(__name, __super, __ivars_decls) \
extern Class k##__name##Class; \
typedef struct _##__name { __super __ivars_decls } __name


// Generates a forward declaration for the class named '__name'. Additionally
// declares a reference type of the form '__nameRef'. This type can be used to
// represent a pointer to an instance of the class.
#define CLASS_FORWARD(__name) \
struct _##__name; \
typedef struct _##__name* __name##Ref


// Declares an open root class. An open class can be subclassed. Defines a
// struct that declares all class ivars plus a reference type for the class.
#define OPEN_ROOT_CLASS_WITH_REF(__name, __ivar_decls) \
__CLASS_IVARS(__name, , __ivar_decls); \
typedef struct _##__name* __name##Ref

// Defines the method table of a root class. Expects the class name and a list
// of METHOD_IMPL macros with one macro per dynamically dispatched method.
#define ROOT_CLASS_METHODS(__name, ...) \
__CLASS_METHODS(__name, NULL, __VA_ARGS__)


// Declares an open class. An open class can be subclassed. Note that this variant
// does not define the __nameRef type.
#define OPEN_CLASS(__name, __super, __ivar_decls)\
__CLASS_IVARS(__name, __super super;, __ivar_decls)

// Same as OPEN_CLASS but also defines a __nameRef type.
#define OPEN_CLASS_WITH_REF(__name, __super, __ivar_decls)\
__CLASS_IVARS(__name, __super super;, __ivar_decls); \
typedef struct _##__name* __name##Ref


// Defines an opaque class. An opaque class supports limited subclassing only.
// Overriding methods is supported but adding ivars is not. This macro should
// be placed in the publicly accessible header file of the class.
#define OPAQUE_CLASS(__name, __superName) \
struct _##__name; \
typedef struct _##__name* __name##Ref

// Defines the ivars of an opaque class. This macro should be placed either in
// the class implementation file or a private class header file.
#define CLASS_IVARS(__name, __super, __ivar_decls) \
__CLASS_IVARS(__name, __super super;, __ivar_decls)


// Defines the method table of an open or opaque class. This macro expects a list
// of METHOD_IMPL or OVERRIDE_METHOD_IMPL macros and it should be placed in the
// class implementation file.
#define CLASS_METHODS(__name, __super, ...) \
__CLASS_METHODS(__name, &k##__super##Class, __VA_ARGS__)


// Defining an open class:
//
// 1. In the (public) .h file:
// 1.a OPEN_CLASS
// 1.b define a struct __classNameMethodTable with the dynamically dispatched methods that the class defines
//
// 2. In the .c file:
// 2.a add the implementation of dynamically dispatched methods
// 2.b CLASS_METHODS with one METHOD_IMPL per dynamically dispatched method
//
//
// Defining an opaque class:
//
// 1. In the (public) .h file:
// 1.a OPAQUE_CLASS
// 1.b define a struct __classNameMethodTable with the dynamically dispatched methods that the class defines
//
// 2. In the (private) .h file:
// 2.a CLASS_IVARS
//
// 3. In the .c file:
// 3.a add the implementation of dynamically dispatched methods
// 3.b CLASS_METHODS with one METHOD_IMPL per dynamically dispatched method
//

typedef void (*Method)(void* _Nonnull self, ...);

struct MethodDecl {
    Method _Nonnull method;
    ByteCount       offset;
};

#define CLASSF_INITIALIZED  1

typedef struct _Class {
    Method* _Nonnull                    vtable;
    struct _Class* _Nonnull             super;
    const char* _Nonnull                name;
    ByteCount                           instanceSize;
    Int16                               methodCount;
    UInt16                              flags;
    const struct MethodDecl* _Nonnull   methodList;
} Class;
typedef struct _Class* ClassRef;


// The Object class. This is the root class aka top type of the object system.
// All other classes derive directly or indirectly from Object. The only
// operations supported by Object are reference counting based memory management
// and dynamic method invocation.
// Dynamic method invocation is implemented via index-based method dispatching.
// Every method is assigned an index and this index is used to look up the
// implementation of a method.
OPEN_ROOT_CLASS_WITH_REF(Object,
    ClassRef _Nonnull   clazz;
    AtomicInt           retainCount;
);

typedef struct _ObjectMethodTable {
    void    (*deinit)(void* _Nonnull self);
} ObjectMethodTable;


// Allocates an instance of the given class. 'extraByteCount' is the number of
// extra bytes that should be allocated for the instance on top of the instance
// size recorded in the class. Returns an error if allocation has failed for
// some reason. The returned object has a reference count of 1. All object
// instance variables are initially set to 0.
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
    ((Class*)(((ObjectRef)(__self))->clazz))


// Returns true if the given object is an instance of the given class or one of
// the super classes.
extern Bool _Object_InstanceOf(ObjectRef _Nonnull pObj, ClassRef _Nonnull pTargetClass);

#define Object_InstanceOf(__self, __className) \
    _Object_InstanceOf(__self, &k##__className##Class)


// Invokes a dynamic method with the given arguments.
#define Object_Invoke0(__methodName, __methodClassName, __self) \
    ((struct _##__methodClassName##MethodTable *)(Object_GetClass(__self)->vtable))->__methodName(__self)

#define Object_InvokeN(__methodName, __methodClassName, __self, ...) \
    ((struct _##__methodClassName##MethodTable *)(Object_GetClass(__self)->vtable))->__methodName(__self, __VA_ARGS__)


// Scans the "__class" data section bounded by the '_class' and '_eclass' linker
// symbols for class records and:
// - builds the vtable for each class
// - validates the vtable
// Must be called after the DATA and BSS segments have been established and before
// any code is invoked that might use objects.
// Note that this function is not concurrency safe.
extern void RegisterClasses(void);

// Prints all registered classes
// Note that this function is not concurrency safe.
extern void PrintClasses(void);

#endif /* Object_h */
