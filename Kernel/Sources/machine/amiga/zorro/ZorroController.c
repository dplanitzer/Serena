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
#include <driver/DriverManager.h>


final_class_ivars(ZorroController, Driver,
    ZorroBus    bus;
    CatalogId   busDirId;
);


errno_t ZorroController_Create(CatalogId parentDirId, ZorroControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(ZorroController), 0, parentDirId, (DriverRef*)pOutSelf);
}

static errno_t ZorroController_DetectDevices(ZorroControllerRef _Nonnull _Locked self)
{
    List_ForEach(&self->bus.boards, ZorroBoard,
        const zorro_conf_t* cfg = &pCurNode->cfg;
    
        if (cfg->type == BOARD_TYPE_RAM && cfg->start && cfg->logicalSize > 0) {
            DriverRef dp;
    
            if (ZRamDriver_Create(self->busDirId, cfg, &dp) == EOK) {
                Driver_StartAdoptChild((DriverRef)self, dp);
            }
        }
    );
    
    return EOK;
}

errno_t ZorroController_onStart(ZorroControllerRef _Nonnull _Locked self)
{
    decl_try_err();

    DirEntry be;
    be.dirId = Driver_GetParentDirectoryId(self);
    be.name = "zorro-bus";
    be.uid = kUserId_Root;
    be.gid = kGroupId_Root;
    be.perms = perm_from_octal(0755);

    try(DriverManager_CreateDirectory(gDriverManager, &be, &self->busDirId));

    DriverEntry de;
    de.dirId = self->busDirId;
    de.name = "self";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.handler = NULL;
    de.driver = (DriverRef)self;
    de.arg = 0;

    try(DriverManager_Publish(gDriverManager, &de));

    // Auto config the Zorro bus
    zorro_auto_config(&self->bus);


    // Instantiate the board drivers
    err = ZorroController_DetectDevices(self);

catch:
    if (err != EOK) {
        DriverManager_Unpublish(gDriverManager, (DriverRef)self);
        DriverManager_RemoveDirectory(gDriverManager, self->busDirId);
    }
    return err;
}

void ZorroController_onStop(DriverRef _Nonnull _Locked self)
{
    DriverManager_Unpublish(gDriverManager, self);
}


class_func_defs(ZorroController, Driver,
override_func_def(onStart, ZorroController, Driver)
override_func_def(onStop, ZorroController, Driver)
);
