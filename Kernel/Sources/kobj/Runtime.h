//
//  Runtime.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Runtime_h
#define Runtime_h

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
{ (MethodImpl) __className##_##__name, offsetof(struct __className##MethodTable, __name) },

// Same as METHOD_IMPL except that it declares the method '__name' to be an override
// of the method with the same name and type signature which was originally defined
// in the class '__superClassName'.
#define OVERRIDE_METHOD_IMPL(__name, __className, __superClassName) \
{ (MethodImpl) __className##_##__name, offsetof(struct __superClassName##MethodTable, __name) },


#define __CLASS_METHODS(__name, __super, ...) \
static struct __name##MethodTable g##__name##VTable; \
static const struct MethodDecl g##__name##MDecls[] = { __VA_ARGS__ {NULL, 0} }; \
Class _ClassSection k##__name##Class = {(MethodImpl*)&g##__name##VTable, __super, #__name, sizeof(__name), 0, (uint16_t) sizeof(struct __name##MethodTable)/sizeof(MethodImpl), g##__name##MDecls}

#define __CLASS_IVARS(__name, __super, __ivars_decls) \
extern Class k##__name##Class; \
typedef struct __name { __super __ivars_decls } __name


// Generates a forward declaration for the class named '__name'. Additionally
// declares a reference type of the form '__nameRef'. This type can be used to
// represent a pointer to an instance of the class.
#define CLASS_FORWARD(__name) \
struct __name; \
typedef struct __name* __name##Ref


// Declares an open root class. An open class can be subclassed. Defines a
// struct that declares all class ivars plus a reference type for the class.
#define OPEN_ROOT_CLASS_WITH_REF(__name, __ivar_decls) \
__CLASS_IVARS(__name, , __ivar_decls); \
typedef struct __name* __name##Ref

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
typedef struct __name* __name##Ref


// Defines an opaque class. An opaque class supports limited subclassing only.
// Overriding methods is supported but adding ivars is not. This macro should
// be placed in the publicly accessible header file of the class.
#define OPAQUE_CLASS(__name, __superName) \
extern Class k##__name##Class; \
struct __name; \
typedef struct __name* __name##Ref

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

typedef void (*MethodImpl)(void* _Nonnull self, ...);

typedef struct MethodDecl {
    MethodImpl _Nonnull method;
    size_t              offset;
} MethodDecl;


#define CLASSF_INITIALIZED  1

typedef struct Class {
    MethodImpl* _Nonnull        vtable;
    struct Class* _Nonnull      super;
    const char* _Nonnull        name;
    size_t                      instanceSize;
    uint16_t                    flags;
    uint16_t                    methodCount;
    const MethodDecl* _Nonnull  methodList;
} Class;
typedef struct Class* ClassRef;


#ifndef __DISKIMAGE__
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
#else
extern void _RegisterClass(ClassRef _Nonnull pClass);
#endif /* __DISKIMAGE__ */

#endif /* Runtime_h */
