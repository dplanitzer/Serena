//
//  ZRamDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZRamDriver.h"
#include "ZorroDevice.h"
#include <hal/sys_desc.h>
#include <kern/kalloc.h>

IOCATS_DEF(g_cats, IOMEM_RAM);


errno_t ZRamDriver_Create(IODriverRef _Nullable * _Nonnull pOutSelf)
{
    return IODriver_Create(class(ZRamDriver), 0, g_cats, pOutSelf);
}

errno_t ZRamDriver_start(ZRamDriverRef _Nonnull self)
{
    ZorroDeviceRef zdp = IODriver_GetProvider(self);
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
    return ZorroDevice_GetConfiguration(IODriver_GetProvider(self))->logicalSize;
}

class_func_defs(ZRamDriver, IODriver,
override_func_def(start, ZRamDriver, IODriver)
);
