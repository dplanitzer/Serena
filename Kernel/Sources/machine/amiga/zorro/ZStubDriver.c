//
//  ZStubDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ZStubDriver.h"


errno_t ZStubDriver_Create(ZorroDriverRef _Nonnull zdp, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ZStubDriverRef self;

    err = Driver_Create(class(ZStubDriver), 0, Driver_GetParentDirectoryId(zdp), (DriverRef*)&self);
    if (err == EOK) {
        self->card = zdp;
    }

    *pOutSelf = (DriverRef)self;
    return err;
}

errno_t ZStubDriver_onStart(ZStubDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    const zorro_conf_t* cfg = ZorroDriver_GetConfiguration(self->card);
    char name[6];

    name[0] = 'c';
    name[1] = 'a';
    name[2] = 'r';
    name[3] = 'd';
    name[4] = '0' + cfg->slot;
    name[5] = '\0';

    DriverEntry de;
    de.dirId = Driver_GetParentDirectoryId(self->card);
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0);
    de.category = 0;
    de.driver = (HandlerRef)self;
    de.arg = 0;

    return Driver_Publish(self, &de);
}

void ZStubDriver_onStop(DriverRef _Nonnull _Locked self)
{
    Driver_Unpublish(self);
}

class_func_defs(ZStubDriver, Driver,
override_func_def(onStart, ZStubDriver, Driver)
override_func_def(onStop, ZStubDriver, Driver)
);
