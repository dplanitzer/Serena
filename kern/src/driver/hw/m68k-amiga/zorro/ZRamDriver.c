//
//  ZRamDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZRamDriver.h"
#include <hal/sys_desc.h>
#include <kern/kalloc.h>

IOCATS_DEF(g_cats, IOMEM_RAM);


errno_t ZRamDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(ZRamDriver), 0, g_cats, pOutSelf);
}

errno_t ZRamDriver_onStart(ZRamDriverRef _Nonnull _Locked self)
{
    ZorroDeviceRef zdp = Driver_GetParent(self);
    const zorro_conf_t* cfg = ZorroDevice_GetConfiguration(zdp);
    mem_desc_t md = {0};

    md.lower = cfg->start;
    md.upper = cfg->start + cfg->logicalSize;
    md.type = MEM_TYPE_MEMORY;
    (void) kalloc_add_memory_region(&md);

    return EOK;
}

size_t ZRamDriver_GetMemorySize(ZRamDriverRef _Nonnull self)
{
    return ZorroDevice_GetConfiguration(Driver_GetParent(self))->logicalSize;
}

class_func_defs(ZRamDriver, Driver,
override_func_def(onStart, ZRamDriver, Driver)
);
