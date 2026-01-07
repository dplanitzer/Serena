//
//  Class.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Class_h
#define Class_h

#include <_cmndef.h>
#include <stdint.h>
#include <stddef.h>


// This section stores all class data structures. The class data structures are
// defined by the macros below.
#ifdef __VBCC__
#define _ClassSection __section("__class")
#else
#define _ClassSection
#endif


#define __CLASS_METHOD_DEFS(__name, __super, ...) \
static struct __name##MethodTable g##__name##VTable; \
static const struct MethodDecl g##__name##MDecls[] = { __VA_ARGS__ {NULL, 0} }; \
Class _ClassSection k##__name##Class = {(MethodImpl*)&g##__name##VTable, __super, #__name, sizeof(struct __name), 0, (uint16_t) sizeof(struct __name##MethodTable)/sizeof(MethodImpl), g##__name##MDecls}

#define __CLASS_METHOD_DECLS(__name, __super, __func_decls) \
struct __name##MethodTable { __super __func_decls }

#define __CLASS_IVARS_DECLS(__name, __super, __ivars_decls) \
extern Class k##__name##Class; \
struct __name { __super __ivars_decls }


// Generates a definition of the reference type of a class instance, __nameRef.
// Add a class_ref() to kobj/AnyRefs.h for each class you define in the kernel. 
#define class_ref(__name) \
typedef struct __name* __name##Ref


// Used to declare the Any type.
#define any_class(__name) \
extern Class k##__name##Class; \
struct __name { Class* _Nonnull clazz; }

// Used to provide the definition of the Any type.
#define any_class_def(__name) \
static void* g##__name##VTable[1] = {(void*)0x5555}; \
static const struct MethodDecl g##__name##MDecls[1] = { {NULL, 0} }; \
Class _ClassSection k##__name##Class = {(MethodImpl*)g##__name##VTable, NULL, #__name, sizeof(struct __name), 0, 0, g##__name##MDecls}


// Declares the methods of a class that is a direct descendent of the Any class.
// This macro should be placed after the class declaration.
#define any_subclass_funcs(__name, __func_decls) \
__CLASS_METHOD_DECLS(__name, , __func_decls)

// Defines the method table of a class that is a direct descendent of the Any
// class. Expects the class name and a list of func_def macros with one macro
// per dynamically dispatched method.
#define any_subclass_func_defs(__name, ...) \
__CLASS_METHOD_DEFS(__name, NULL, __VA_ARGS__)


// Declares an open class. An open class can be subclassed. Note that this variant
// does not define the __nameRef type.
#define open_class(__name, __super, __ivar_decls) \
__CLASS_IVARS_DECLS(__name, struct __super super;, __ivar_decls)

// Defines the methods of a open class. This macro should be placed after the
// open_class() macro.
#define open_class_funcs(__name, __super, __func_decls) \
__CLASS_METHOD_DECLS(__name, struct __super##MethodTable super;, __func_decls)


// Defines a final class. A final class does not support subclassing. Additionally
// all class ivars are defined in a separate private header file or the class
// implementation file.
// This macro should be placed in the publicly accessible header file of the class.
#define final_class(__name, __superName) \
extern Class k##__name##Class; \
struct __name; \
struct __name##MethodTable { struct __superName##MethodTable super; }

// Defines the ivars of a final class. This macro should be placed either in
// the class implementation file or a private class header file.
#define final_class_ivars(__name, __super, __ivar_decls) \
__CLASS_IVARS_DECLS(__name, struct __super super;, __ivar_decls)


// Defines the method table of an open or final class. This macro expects a list
// of func_def() or override_func_def() macros and it should be placed in the
// class implementation file.
#define class_func_defs(__name, __super, ...) \
__CLASS_METHOD_DEFS(__name, &k##__super##Class, __VA_ARGS__)

// A method implementation declaration. This adds the method named '__name' of
// the class named '__className' to this class. Use this macro to declare a
// method that does not override the equally named superclass method.
#define func_def(__name, __className) \
{ (MethodImpl) __className##_##__name, offsetof(struct __className##MethodTable, __name) },

// Same as func_def except that it declares the method '__name' to be an override
// of the method with the same name and type signature which was originally defined
// in the class '__superClassName'.
#define override_func_def(__name, __className, __superClassName) \
{ (MethodImpl) __className##_##__name, offsetof(struct __superClassName##MethodTable, __name) },


//
// Defining an open class which can be subclassed:
//
// 1. In the (public) .h file:
// 1.a open_class(class_name, super_class_name, ivar, ...)
// 1.b define a struct __className##MethodTable with the dynamically dispatched methods that the class defines
//
// 2. In the .c file:
// 2.a add the implementation of dynamically dispatched methods
// 2.b class_func_defs() with one func_def() or override_func_def() per dynamically dispatched method
//
// 3. In kobj/ObjectRefs.h add a class_ref() for the new class
//
//
// Defining a final class which can not be subclassed:
//
// 1. In the (public) .h file:
// 1.a final_class(class_name, super_class_name)
// 1.b define a struct __className##MethodTable with the dynamically dispatched methods that the class defines
//
// 2. In the (private) .h file:
// 2.a final_class_ivars(ivar, ...)
//
// 3. In the .c file:
// 3.a add the implementation of dynamically dispatched methods
// 3.b class_func_defs() with one func_def() or override_func_def() per dynamically dispatched method
//
// 4. In kobj/ObjectRefs.h add a class_ref() for the new class
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
extern void _RegisterClass(Class* _Nonnull pClass);
#endif /* __DISKIMAGE__ */

#endif /* Class_h */
