//
//  PlatformController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PlatformController.h"

PlatformControllerRef gPlatformController;

IOCATS_DEF(g_cats, IOBUS_VIRTUAL);


errno_t PlatformController_Create(Class* _Nonnull pClass, DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(pClass, 0, g_cats, pOutSelf);
}

errno_t PlatformController_start(PlatformControllerRef _Nonnull _Locked self)
{
    return PlatformController_DetectDevices(self);
}

errno_t PlatformController_detectDevices(PlatformControllerRef _Nonnull _Locked self)
{
    return EOK;
}

uint64_t PlatformController_getPhysicalMemorySize(PlatformControllerRef _Nonnull self)
{
    return 0ull;
}

const struct SMG_Header* _Nullable PlatformController_getBootImage(PlatformControllerRef _Nonnull self)
{
    return EOK;
}


class_func_defs(PlatformController, Driver,
override_func_def(start, PlatformController, Driver)
func_def(detectDevices, PlatformController)
func_def(getPhysicalMemorySize, PlatformController)
func_def(getBootImage, PlatformController)
);
