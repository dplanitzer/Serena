//
//  PlatformController.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef PlatformController_h
#define PlatformController_h

#include <kobj/Object.h>


// A platform controller is responsible for setting up the root drivers of a
// platform. The root drivers are typically drivers who are managing motherboard
// components. I.e. built-in floppy disks, graphic chips, sound chips, expansion
// boards like Zorro, PCI, ISA, EISA, etc.
open_class(PlatformController, Object,
);
open_class_funcs(PlatformController, Object,
    errno_t (*autoConfigureForConsole)(PlatformControllerRef _Nonnull self, DriverCatalogRef _Nonnull catalog);
    errno_t (*autoConfigure)(PlatformControllerRef _Nonnull self, DriverCatalogRef _Nonnull catalog);
);

#define PlatformController_AutoConfigureForConsole(__self, __catalog) \
invoke_n(autoConfigureForConsole, PlatformController, __self, __catalog)

#define PlatformController_AutoConfigure(__self, __catalog) \
invoke_n(autoConfigure, PlatformController, __self, __catalog)

#endif /* PlatformController_h */
