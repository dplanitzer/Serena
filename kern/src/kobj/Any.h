//
//  Any.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Any_h
#define Any_h

#include <stdbool.h>
#include <kobj/Class.h>
#include <kobj/AnyRefs.h>


// The Any type. This is the root type aka top type of the object type system.
// All other classes derive directly or indirectly from Any. The only
// operations supported by Any are subclassing, evaluation of subclass
// relationships and dynamic method dispatching.
// Any does not define any dynamically dispatched operations and no memory
// management model. This definition of Any leaves a tremendous amount of power
// to subtypes to define their respective behavior in precisely scoped and
// efficient ways. This is also why we say that Any's definition is 'pure'.
//
// Note that the Any type is an abstract type which can not be instantiated.
//
// Dynamic method dispatching is implemented via index-based method dispatch
// table. Every method is statically assigned an index and this index is used to
// look up the implementation of a method at runtime.
any_class(Any);


// Returns a pointer to the class structure for the type '__name'
#define class(__name) \
((Class*)(&k##__name##Class))


// Returns the class of the given instance.
#define classof(__self)\
    ((Class*)(((AnyRef)(__self))->clazz))


// Returns the superclass of the given instance.
#define superclassof(__self)\
    classof(__self)->super


// Returns true if the given object is an instance of the given class or one of
// the super classes.
#define instanceof(__self, __className) \
    _instanceof((AnyRef)__self, class(__className))


// Returns the implementation pointer of a method. You must cast this pointer to
// the correct method signature in order to invoke it correctly.
#define implementationof(__func, __className, __class) \
    (MethodImpl)((struct __className##MethodTable*)((__class)->vtable))->__func


// Non-resilient, inline method dispatcher
#define dispatch_0(__func, __className, __class, __self) \
    ((struct __className##MethodTable*)((__class)->vtable))->__func(__self)

#define dispatch_n(__func, __className, __class, __self, ...) \
    ((struct __className##MethodTable*)((__class)->vtable))->__func(__self, __VA_ARGS__)


// Invokes a dynamically dispatched method with no arguments besides self.
#define invoke_0(__func, __className, __self) \
    dispatch_0(__func, __className, classof(__self), __self)

// Invokes a dynamically dispatched method with the given arguments.
#define invoke_n(__func, __className, __self, ...) \
    dispatch_n(__func, __className, classof(__self), __self, __VA_ARGS__)


// Invokes a dynamically dispatched method with the given arguments on the superclass of the receiver.
// '__func' is the function to invoke
// '__funcClassName' is the name of the class that introduces (contains the first definition) of '__func'
// '__className' is the static type of the instance (the name of the class in which the super() call appears)
// '__self' is the instance on which to invoke the function
#define super_0(__func, __funcClassName, __className, __self) \
    dispatch_0(__func, __funcClassName, _superimplementationof(class(__className), offsetof(struct __funcClassName##MethodTable, __func)), __self)

#define super_n(__func, __funcClassName, __className, __self, ...) \
    dispatch_n(__func, __funcClassName, _superimplementationof(class(__className), offsetof(struct __funcClassName##MethodTable, __func)), __self, __VA_ARGS__)



// Do not call these functions directly. Use the macros defined above instead.
extern bool _instanceof(AnyRef _Nonnull self, const Class* _Nonnull targetType);
extern Class* _Nonnull _superimplementationof(Class* _Nonnull staticType, size_t methodOffset);

#endif /* Any_h */
