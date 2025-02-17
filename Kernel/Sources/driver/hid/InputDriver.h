//
//  InputDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef InputDriver_h
#define InputDriver_h

#include <klib/klib.h>
#include <driver/Driver.h>
#include <System/HID.h>


// An input driver manages a specific input device and translates actions on the
// input device into events that it the posts to the HID manager.
//
open_class(InputDriver, Driver,
);
open_class_funcs(InputDriver, Driver,

    // Returns information about the input drive.
    // Override: optional
    // Default Behavior: returns info for a null input driver
    void (*getInfo)(void* _Nonnull _Locked self, InputInfo* _Nonnull pOutInfo);

    // Returns the input driver type.
    // Override: required
    // Default Behavior: returns kInputType_None
    InputType (*getInputType)(void* _Nonnull _Locked self);
);


//
// Methods for use by disk driver users.
//

extern errno_t InputDriver_GetInfo(InputDriverRef _Nonnull self, InputInfo* pOutInfo);

//
// Subclassers
//

#define InputDriver_GetInputType(__self) \
((InputType) invoke_0(getInputType, InputDriver, __self))


#endif /* InputDriver_h */
