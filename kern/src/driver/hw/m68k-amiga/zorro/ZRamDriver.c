//
//  ZRamDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZRamDriver.h"
#include "ZorroDevice.h"

IOCATS_DEF(g_cats, IOMEM_RAM);


errno_t ZRamDriver_Create(IODriverRef _Nullable * _Nonnull pOutSelf)
{
    return IODriver_Create(class(ZRamDriver), g_cats, pOutSelf);
}

errno_t ZRamDriver_start(ZRamDriverRef _Nonnull self)
{
    return IODriver_Open(IODriver_GetProvider(self), O_RDWR);
}

bool ZRamDriver_stop(ZRamDriverRef _Nonnull self)
{
    IODriver_Close(IODriver_GetProvider(self));
    return true;
}

void* _Nonnull ZRamDriver_GetPhysicalBaseAddress(ZRamDriverRef _Nonnull self)
{
    return ZorroDevice_GetConfiguration(IODriver_GetProvider(self))->start;
}

size_t ZRamDriver_GetMemorySize(ZRamDriverRef _Nonnull self)
{
    return ZorroDevice_GetConfiguration(IODriver_GetProvider(self))->logicalSize;
}

class_func_defs(ZRamDriver, IODriver,
override_func_def(start, ZRamDriver, IODriver)
override_func_def(stop, ZRamDriver, IODriver)
);
