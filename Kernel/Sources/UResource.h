//
//  UResource.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef UResource_h
#define UResource_h

#include <kobj/Any.h>
#include <dispatcher/Lock.h>


// UResource ownership and operations tracking:
//
// UResource objects are owned by a process' UResourceTable. They do not support
// reference counting for ownership purposes. A UResource is created with the
// resource subclass specific create() function and it is freed by calling
// UResource_Dispose(). Once a resource has been disposed and there are no more
// ongoing operations on the resource, and subject to the requirements of the
// resource dispose mode (see below) the resource is deinitialized and deallocated.
//
// Operations on a resource are tracked with the UResource_BeginOperation()
// and UResource_EndOperation() calls. The former should be called before invoking
// one or more resource operations and the latter one should be called at the
// end of a sequence of resource operation calls.
//
// The UResourceTable in a process takes care of the ownership of a resource.
// It also provides the UResourceTable_AcquireResource() and
// UResourceTable_RelinquishResource() calls to take care of the resource
// operation tracking.
//
// 
// Behavior of the dispose() system call:
//
// The dispose() system call schedules a resource for deallocation and it removes
// the user visible descriptor/name of the resource from the process' resource
// table.
//
//
// The two resource dispose modes:
//
// 1) The dispose() system call makes the resource invisible to user space and it
//    schedules it for deallocation. However the actual deallocation is deferred
//    until all still ongoing resource operations have completed.
//    (dispose with deferred deallocation mode)
//
// 2) Similar to (1), however all ongoing resource operations are canceled by the
//    dispose() invocation and the resource is deallocated as soon as all cancel
//    operations have completed.
//    (canceling dispose mode)
//
// Only mode (1) is supported by the resource class at this time. Support for
// the other modes is planned for the future.
open_class_with_ref(UResource, Any,
    Lock        countLock;
    int32_t     useCount;
    uint32_t    flags;
);
any_subclass_funcs(UResource,
    // Called once the deallocation of a resource has been triggered. Subclassers
    // should override this method and deallocate all resources used by the
    // UResource implementation.
    // Subclassers should not invoke the super implementation themselves. This is
    // taken care of automatically.
    void (*deinit)(void* _Nonnull self);
);


// UResource functions for use by subclassers

// Creates an instance of a UResource. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
extern errno_t UResource_AbstractCreate(Class* _Nonnull pClass, UResourceRef _Nullable * _Nonnull pOutSelf);


// UResource functions for use by UResourceTable

#define UResource_Dispose(__self) \
_UResource_Dispose((UResourceRef)__self)

#define UResource_BeginOperation(__self) \
_UResource_BeginOperation((UResourceRef)__self)

#define UResource_EndOperation(__self) \
_UResource_EndOperation((UResourceRef)__self)


// Do not call these functions directly. Use the macros above instead

extern void _UResource_Dispose(UResourceRef _Nullable self);
extern void _UResource_BeginOperation(UResourceRef _Nonnull self);
extern void _UResource_EndOperation(UResourceRef _Nonnull self);

#endif /* UResource_h */
