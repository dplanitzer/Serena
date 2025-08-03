//
//  PlatformController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PlatformController.h"
#include "DriverManager.h"

PlatformControllerRef gPlatformController;


errno_t PlatformController_Create(Class* _Nonnull pClass, DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(pClass, 0, kCatalogId_None, pOutSelf);
}

errno_t PlatformController_onStart(PlatformControllerRef _Nonnull _Locked self)
{
    decl_try_err();

    DirEntry de;
    de.dirId = kCatalogId_None;
    de.name = "hw";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0755);

    try(DriverManager_CreateDirectory(gDriverManager, &de, &self->hardwareDirectoryId));
    try(PlatformController_DetectDevices(self));

catch:
    return err;
}

errno_t PlatformController_detectDevices(PlatformControllerRef _Nonnull _Locked self)
{
    return EOK;
}

const struct SMG_Header* _Nullable PlatformController_getBootImage(PlatformControllerRef _Nonnull self)
{
    return EOK;
}


class_func_defs(PlatformController, Driver,
override_func_def(onStart, PlatformController, Driver)
func_def(detectDevices, PlatformController)
func_def(getBootImage, PlatformController)
);
