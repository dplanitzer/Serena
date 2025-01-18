//
//  ZRamDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZRamDriver.h"
#include <klib/Kalloc.h>


errno_t ZRamDriver_Create(const ZorroConfiguration* _Nonnull config, DriverRef _Nullable * _Nonnull pOutSelf)
{
    return ZorroDriver_Create(ZRamDriver, 0, config, pOutSelf);
}

errno_t ZRamDriver_onStart(DriverRef _Nonnull _Locked self)
{
    decl_try_err();
    const ZorroConfiguration* cfg = ZorroDriver_GetBoardConfiguration(self);
    MemoryDescriptor md = {0};
    char name[5];

    name[0] = 'r';
    name[1] = 'a';
    name[2] = 'm';
    name[3] = '0' + cfg->slot;
    name[4] = '\0';

    if ((err = Driver_Publish((DriverRef)self, name, 0)) == EOK) {
        md.lower = cfg->start;
        md.upper = cfg->start + cfg->logicalSize;
        md.type = MEM_TYPE_MEMORY;
        (void) kalloc_add_memory_region(&md);
    }

    return err;
}

class_func_defs(ZRamDriver, Driver,
override_func_def(onStart, ZRamDriver, Driver)
);
