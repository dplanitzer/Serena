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

IOCATS_DEF(g_cats, IOBUS_VIRTUAL);


errno_t PlatformController_Create(Class* _Nonnull pClass, DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_CreateRoot(pClass, kDriver_IsBus, g_cats, pOutSelf);
}

errno_t PlatformController_onStart(PlatformControllerRef _Nonnull _Locked self)
{
    decl_try_err();

    DirEntry be;
    be.dirId = kCatalogId_None;
    be.name = "hw";
    be.uid = kUserId_Root;
    be.gid = kGroupId_Root;
    be.perms = perm_from_octal(0755);

    try(Driver_PublishBusDirectory((DriverRef)self, &be));
    self->hardwareDirectoryId = Driver_GetPublishedBusDirectory(self);
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
