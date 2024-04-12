//
//  Any.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Any_h
#define Any_h

#include <klib/Atomic.h>
#include <klib/Error.h>
#include <kobj/Runtime.h>


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


// Returns the class of the given instance.
#define classof(__self)\
    ((Class*)(((Any*)(__self))->clazz))


// Returns the superclass of the given instance.
#define superclassof(__self)\
    classof(__self)->super


// Returns true if the given object is an instance of the given class or one of
// the super classes.
#define instanceof(__self, __className) \
    _instanceof((Any*)__self, &k##__className##Class)


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
#define super_0(__func, __superClassName, __self) \
    dispatch_0(__func, __superClassName, superclassof(__self), __self)

#define super_n(__func, __superClassName, __self, ...) \
    dispatch_n(__func, __superClassName, superclassof(__self), __self, __VA_ARGS__)



// Do not call these functions directly. Use the macros defined above instead.
extern bool _instanceof(Any* _Nonnull pObj, Class* _Nonnull pTargetType);

#endif /* Any_h */
