//
//  PlatformController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PlatformController.h"
#include "DriverCatalog.h"


DriverRef gPlatformController;


errno_t PlatformController_Create(Class* _Nonnull pClass, DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(pClass, 0, NULL, pOutSelf);
}

errno_t PlatformController_PublishHardwareBus(PlatformControllerRef _Nonnull _Locked self)
{
    return DriverCatalog_PublishBus(gDriverCatalog, kDriverCatalogId_None, "hw", kUserId_Root, kGroupId_Root, FilePermissions_MakeFromOctal(0755), &((DriverRef)self)->busCatalogId);
}

class_func_defs(PlatformController, Driver,
);
