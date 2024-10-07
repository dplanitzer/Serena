//
//  PlatformController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PlatformController.h"


errno_t PlatformController_autoConfigureForConsole(PlatformControllerRef _Nonnull self, DriverCatalogRef _Nonnull catalog)
{
    return ENODEV;
}

errno_t PlatformController_autoConfigure(PlatformControllerRef _Nonnull self, DriverCatalogRef _Nonnull catalog)
{
    return ENODEV;
}

class_func_defs(PlatformController, Driver,
    func_def(autoConfigureForConsole, PlatformController)
    func_def(autoConfigure, PlatformController)
);
