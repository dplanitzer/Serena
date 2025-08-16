//
//  ZorroController.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ZorroController.h"
#include "ZorroDriver.h"
#include "zorro_bus.h"
#include <driver/DriverManager.h>


final_class_ivars(ZorroController, Driver,
    CatalogId   busDirId;
);


errno_t ZorroController_Create(CatalogId parentDirId, ZorroControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(ZorroController), 0, parentDirId, (DriverRef*)pOutSelf);
}

static errno_t _auto_config_bus(ZorroControllerRef _Nonnull _Locked self)
{
    decl_try_err();
    zorro_bus_t bus;

    // Auto config the Zorro bus
    zorro_auto_config(&bus);
    try(Driver_SetMaxChildCount((DriverRef)self, bus.count));


    // Create a ZorroDriver instance for each slot
    List_ForEach(&bus.boards, zorro_board_t,
        const zorro_conf_t* cfg = &pCurNode->cfg;
        ZorroDriverRef dp;
        
        if (ZorroDriver_Create(cfg, self->busDirId, &dp) == EOK) {
            Driver_AdoptChild((DriverRef)self, (DriverRef)dp);
        }
    );
    

    // Start the slot drivers
    for (size_t slot = 0; slot < bus.count; slot++) {
        DriverRef dp = Driver_GetChildAt((DriverRef)self, slot);

        if (dp) {
            Driver_Start(dp);
        }
    }


catch:
    zorro_destroy_bus(&bus);
    return err;
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
    de.category = 0;
    de.driver = (HandlerRef)self;
    de.arg = 0;

    try(Driver_Publish(self, &de));


    // Auto-config the bus
    err = _auto_config_bus(self);

catch:
    if (err != EOK) {
        Driver_Unpublish(self);
        DriverManager_RemoveDirectory(gDriverManager, self->busDirId);
    }
    return err;
}

void ZorroController_onStop(DriverRef _Nonnull _Locked self)
{
    Driver_Unpublish(self);
}

errno_t ZorroController_ioctl(ZorroControllerRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kZorroCommand_GetCardCount: {
            size_t* pCount = va_arg(ap, size_t*);
            
            *pCount = Driver_GetCurrentChildCount((DriverRef)self);
            return EOK;
        }

        case kZorroCommand_GetCardConfig: {
            size_t idx = va_arg(ap, size_t);
            zorro_conf_t* pcfg = va_arg(ap, zorro_conf_t*);
            ZorroDriverRef zdp = (ZorroDriverRef)Driver_GetChildAt((DriverRef)self, idx);
        
            if (zdp) {
                *pcfg = *ZorroDriver_GetConfiguration(zdp);
                return EOK;
            }
            else {
                return EINVAL;
            }
        }

        default:
            return super_n(ioctl, Handler, ZorroController, self, pChannel, cmd, ap);
    }
}


class_func_defs(ZorroController, Driver,
override_func_def(onStart, ZorroController, Driver)
override_func_def(onStop, ZorroController, Driver)
override_func_def(ioctl, ZorroController, Handler)
);
