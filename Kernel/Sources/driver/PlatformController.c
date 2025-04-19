//
//  PlatformController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PlatformController.h"
#include <Catalog.h>


DriverRef gPlatformController;


errno_t PlatformController_Create(Class* _Nonnull pClass, DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(pClass, 0, NULL, pOutSelf);
}

errno_t PlatformController_onStart(PlatformControllerRef _Nonnull _Locked self)
{
    decl_try_err();

    try(Catalog_PublishFolder(gDriverCatalog, kCatalogId_None, "hw", kUserId_Root, kGroupId_Root, FilePermissions_MakeFromOctal(0755), &((DriverRef)self)->busCatalogId));
    try(invoke_0(detectDevices, PlatformController, self));

catch:
    return err;
}

errno_t PlatformController_detectDevices(PlatformControllerRef _Nonnull _Locked self)
{
    return EOK;
}


class_func_defs(PlatformController, Driver,
override_func_def(onStart, PlatformController, Driver)
func_def(detectDevices, PlatformController)
);
