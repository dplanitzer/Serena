//
//  PlatformController.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef PlatformController_h
#define PlatformController_h

#include <driver/Driver.h>

struct SMG_Header;


// A platform controller is the root driver of a platform. All other drivers are
// direct or indirect children of the platform controller. It represents the
// motherboard hardware in that sense and it kicks off the detection of hardware
// that is part of the motherboard.
// A platform controller is expected to implement the synchronous driver model.
open_class(PlatformController, Driver,
    CatalogId   hardwareDirectoryId;
);
open_class_funcs(PlatformController, Driver,

    // Override in a subclass to detect all relevant devices that are directly
    // connected to the motherboard and instantiate suitable driver classes for
    // them.
    // Override: Required
    // Default Behavior: Does nothing and returns EOK
    errno_t (*detectDevices)(void* _Nonnull self);

    // Override in a subclass to return a Rom-based disk image from which to boot
    // the system off. Return NULL if no such image exists and the system should
    // boot off eg a disk instead.
    const struct SMG_Header* _Nullable (*getBootImage)(void* _Nonnull self);
);


// Creates a platform controller instance.
extern errno_t PlatformController_Create(Class* _Nonnull pClass, DriverRef _Nullable * _Nonnull pOutSelf);


//
// Subclassers
//

#define PlatformController_DetectDevices(__self) \
invoke_0(detectDevices, PlatformController, __self)

#define PlatformController_GetBootImage(__self) \
invoke_0(getBootImage, PlatformController, __self)

// Returns the id of the hardware directory. This is the directory inside of
// which all platform specific drivers should be placed (aka '/dev/hw').
#define PlatformController_GetHardwareDirectoryId(__self) \
((PlatformControllerRef)__self)->hardwareDirectoryId

#endif /* PlatformController_h */
