//
//  ZRamDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZRamDriver.h"
#include <kern/kalloc.h>
#include <machine/Platform.h>


errno_t ZRamDriver_Create(CatalogId parentDirId, const zorro_conf_t* _Nonnull config, DriverRef _Nullable * _Nonnull pOutSelf)
{
    return ZorroDriver_Create(class(ZRamDriver), 0, parentDirId, config, pOutSelf);
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
    de.dirId = Driver_GetParentDirectoryId(self);
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0440);
    de.category = 0;
    de.handler = NULL;
    de.driver = (DriverRef)self;
    de.arg = 0;

    if ((err = Driver_Publish(self, &de)) == EOK) {
        md.lower = cfg->start;
        md.upper = cfg->start + cfg->logicalSize;
        md.type = MEM_TYPE_MEMORY;
        (void) kalloc_add_memory_region(&md);
    }

    return err;
}

void ZRamDriver_onStop(DriverRef _Nonnull _Locked self)
{
    Driver_Unpublish(self);
}

class_func_defs(ZRamDriver, ZorroDriver,
override_func_def(onStart, ZRamDriver, Driver)
override_func_def(onStop, ZRamDriver, Driver)
);
