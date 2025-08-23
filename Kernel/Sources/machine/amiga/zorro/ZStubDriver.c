//
//  ZStubDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZStubDriver.h"

IOCATS_DEF(g_cats, IOUNS_UNKNOWN);


errno_t ZStubDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(ZStubDriver), 0, g_cats, pOutSelf);
}

errno_t ZStubDriver_onStart(ZStubDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    ZorroDriverRef zdp = Driver_GetParentAs(self, ZorroDriver);
    const zorro_conf_t* cfg = ZorroDriver_GetConfiguration(zdp);
    char name[6];

    name[0] = 'c';
    name[1] = 'a';
    name[2] = 'r';
    name[3] = 'd';
    name[4] = '0' + cfg->slot;
    name[5] = '\0';

    DriverEntry de;
    de.dirId = Driver_GetBusDirectory((DriverRef)self);
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0);
    de.arg = 0;

    return Driver_Publish(self, &de);
}

class_func_defs(ZStubDriver, Driver,
override_func_def(onStart, ZStubDriver, Driver)
);
