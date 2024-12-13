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


// A platform controller is the root driver of a platform. All other drivers are
// direct or indirect children of the platform controller. It represents the
// motherboard hardware in that sense and it kicks off the detection of hardware
// that is part of the motherboard.
// A platform controller is expected to implement the synchronous driver model.
open_class(PlatformController, Driver,
);
open_class_funcs(PlatformController, Driver,
);


// Should be invoked by the platform specific subclass to inform the kernel that
// the console is available now. Initializes the kernel logging services and 
// prints the boot banner.
extern void PlatformController_NoteConsoleAvailable(PlatformControllerRef _Nonnull self);

#endif /* PlatformController_h */
