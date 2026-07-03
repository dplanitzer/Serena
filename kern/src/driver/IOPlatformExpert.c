//
//  IOPlatformExpert.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "IOPlatformExpert.h"

IOPlatformExpertRef gIOPlatformExpert;

IOCATS_DEF(g_cats, IOBUS_VIRTUAL);


errno_t IOPlatformExpert_Create(Class* _Nonnull pClass, IODriverRef _Nullable * _Nonnull pOutSelf)
{
    return IODriver_Create(pClass, g_cats, pOutSelf);
}

uint64_t IOPlatformExpert_getPhysicalMemorySize(IOPlatformExpertRef _Nonnull self)
{
    return 0ull;
}

const struct SMG_Header* _Nullable IOPlatformExpert_getBootImage(IOPlatformExpertRef _Nonnull self)
{
    return EOK;
}


class_func_defs(IOPlatformExpert, IODriver,
func_def(getPhysicalMemorySize, IOPlatformExpert)
func_def(getBootImage, IOPlatformExpert)
);
