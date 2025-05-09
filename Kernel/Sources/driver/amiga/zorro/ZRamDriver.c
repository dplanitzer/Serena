//
//  ZRamDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZRamDriver.h"
#include <klib/Kalloc.h>


errno_t ZRamDriver_Create(DriverRef _Nullable parent, const zorro_conf_t* _Nonnull config, DriverRef _Nullable * _Nonnull pOutSelf)
{
    return ZorroDriver_Create(class(ZRamDriver), 0, parent, config, pOutSelf);
}

errno_t ZRamDriver_onStart(DriverRef _Nonnull _Locked self)
{
    decl_try_err();
    const zorro_conf_t* cfg = ZorroDriver_GetBoardConfiguration(self);
    MemoryDescriptor md = {0};
    char name[5];

    name[0] = 'r';
    name[1] = 'a';
    name[2] = 'm';
    name[3] = '0' + cfg->slot;
    name[4] = '\0';

    DriverEntry de;
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = FilePermissions_MakeFromOctal(0440);
    de.arg = 0;

    if ((err = Driver_Publish((DriverRef)self, &de)) == EOK) {
        md.lower = cfg->start;
        md.upper = cfg->start + cfg->logicalSize;
        md.type = MEM_TYPE_MEMORY;
        (void) kalloc_add_memory_region(&md);
    }

    return err;
}

class_func_defs(ZRamDriver, ZorroDriver,
override_func_def(onStart, ZRamDriver, Driver)
);
