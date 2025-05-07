//
//  ZorroController.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ZorroController.h"
#include "zorro_bus.h"
#include "ZRamDriver.h"


final_class_ivars(ZorroController, Driver,
    ZorroBus    bus;
);


errno_t ZorroController_Create(DriverRef _Nullable parent, ZorroControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(ZorroController), 0, parent, (DriverRef*)pOutSelf);
}

static errno_t ZorroController_DetectDevices(ZorroControllerRef _Nonnull _Locked self)
{
    List_ForEach(&self->bus.boards, ZorroBoard,
        const os_zorro_conf_t* cfg = &pCurNode->cfg;
    
        if (cfg->type == BOARD_TYPE_RAM && cfg->start && cfg->logicalSize > 0) {
            DriverRef dp;
    
            if (ZRamDriver_Create((DriverRef)self, cfg, &dp) == EOK) {
                Driver_StartAdoptChild((DriverRef)self, dp);
            }
        }
    );
    
    return EOK;
}

errno_t ZorroController_onStart(ZorroControllerRef _Nonnull _Locked self)
{
    decl_try_err();

    BusEntry be;
    be.name = "zorro-bus";
    be.uid = kUserId_Root;
    be.gid = kGroupId_Root;
    be.perms = FilePermissions_MakeFromOctal(0755);

    DriverEntry de;
    de.name = "self";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = FilePermissions_MakeFromOctal(0666);
    de.arg = 0;

    try(Driver_PublishBus((DriverRef)self, &be, &de));


    // Auto config the Zorro bus
    zorro_auto_config(&self->bus);


    // Instantiate the board drivers
    err = ZorroController_DetectDevices(self);

catch:
    return err;
}


class_func_defs(ZorroController, Driver,
override_func_def(onStart, ZorroController, Driver)
);
