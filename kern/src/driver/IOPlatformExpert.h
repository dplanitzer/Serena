//
//  IOPlatformExpert.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef IOPlatformExpert_h
#define IOPlatformExpert_h

#include <driver/IODriver.h>

struct SMG_Header;


// A platform controller is the root driver of a platform. All other drivers are
// direct or indirect children of the platform controller. It represents the
// motherboard hardware in that sense and it kicks off the detection of hardware
// that is part of the motherboard.
// Subclasses should override onLaunched(), detect motherboard devices there,
// create suitable drivers and launch them.
open_class(IOPlatformExpert, IODriver,
);
open_class_funcs(IOPlatformExpert, IODriver,

    // Override in a subclass to return the amount of physical RAM in the machine.
    // This is the RAM o the motherboard and all expansion boards that is useable
    // by Serena OS.
    // Override: Required
    // Default: Returns 0
    uint64_t (*getPhysicalMemorySize)(void* _Nonnull self);

    // Override in a subclass to return a Rom-based disk image from which to boot
    // the system off. Return NULL if no such image exists and the system should
    // boot off eg a disk instead.
    const struct SMG_Header* _Nullable (*getBootImage)(void* _Nonnull self);
);


extern IOPlatformExpertRef gIOPlatformExpert;

// Creates a platform controller instance.
extern errno_t IOPlatformExpert_Create(Class* _Nonnull pClass, IODriverRef _Nullable * _Nonnull pOutSelf);


//
// Subclassers
//

#define IOPlatformExpert_GetPhysicalMemorySize(__self) \
invoke_0(getPhysicalMemorySize, IOPlatformExpert, __self)

#define IOPlatformExpert_GetBootImage(__self) \
invoke_0(getBootImage, IOPlatformExpert, __self)

#endif /* IOPlatformExpert_h */
