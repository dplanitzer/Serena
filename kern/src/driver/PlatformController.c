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


errno_t PlatformController_Create(Class* _Nonnull pClass, IODriverRef _Nullable * _Nonnull pOutSelf)
{
    return IODriver_Create(pClass, 0, g_cats, pOutSelf);
}

uint64_t PlatformController_getPhysicalMemorySize(PlatformControllerRef _Nonnull self)
{
    return 0ull;
}

const struct SMG_Header* _Nullable PlatformController_getBootImage(PlatformControllerRef _Nonnull self)
{
    return EOK;
}


class_func_defs(PlatformController, IODriver,
func_def(getPhysicalMemorySize, PlatformController)
func_def(getBootImage, PlatformController)
);
