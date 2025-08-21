//
//  ZRamDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZRamDriver.h"
#include <kern/kalloc.h>
#include <machine/sys_desc.h>

IOCATS_DEF(g_cats, IOMEM_RAM);


errno_t ZRamDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(ZRamDriver), 0, 0, g_cats, pOutSelf);
}

errno_t ZRamDriver_onStart(ZRamDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    ZorroDriverRef zdp = Driver_GetParentAs(self, ZorroDriver);
    const zorro_conf_t* cfg = ZorroDriver_GetConfiguration(zdp);
    mem_desc_t md = {0};
    char name[5];

    name[0] = 'r';
    name[1] = 'a';
    name[2] = 'm';
    name[3] = '0' + cfg->slot;
    name[4] = '\0';

    DriverEntry de;
    de.dirId = Driver_GetBusDirectory(zdp);
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0440);
    de.driver = (HandlerRef)self;
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

class_func_defs(ZRamDriver, Driver,
override_func_def(onStart, ZRamDriver, Driver)
override_func_def(onStop, ZRamDriver, Driver)
);
