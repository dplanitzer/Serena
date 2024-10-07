//
//  PlatformController.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef PlatformController_h
#define PlatformController_h

#include <Driver.h>


// A platform controller is the root driver of a platform. All other drivers are
// direct or indirect children of the platform controller. It represents the
// motherboard hardware in that sense and it kicks off the detection of hardware
// that is part of the motherboard.
// A platform controller is expected to implement the synchronous driver model.
open_class(PlatformController, Driver,
);
open_class_funcs(PlatformController, Driver,
    errno_t (*autoConfigureForConsole)(PlatformControllerRef _Nonnull self, DriverCatalogRef _Nonnull catalog);
    errno_t (*autoConfigure)(PlatformControllerRef _Nonnull self, DriverCatalogRef _Nonnull catalog);
);

#define PlatformController_AutoConfigureForConsole(__self, __catalog) \
invoke_n(autoConfigureForConsole, PlatformController, __self, __catalog)

#define PlatformController_AutoConfigure(__self, __catalog) \
invoke_n(autoConfigure, PlatformController, __self, __catalog)

#endif /* PlatformController_h */
