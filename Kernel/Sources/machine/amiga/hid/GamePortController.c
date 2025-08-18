//
//  GamePortController.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "GamePortController.h"
#include "JoystickDriver.h"
#include "LightPenDriver.h"
#include "MouseDriver.h"
#include <driver/DriverManager.h>

IOCATS_DEF(g_cats, IOBUS_GP);


#define GP_PORT_COUNT   2

static errno_t GamePortController_SetPortDevice_Locked(GamePortControllerRef _Nonnull _Locked self, int port, int type);


errno_t GamePortController_Create(CatalogId parentDirId, GamePortControllerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    GamePortControllerRef self;

    try(Driver_Create(class(GamePortController), g_cats, kDriver_Exclusive, parentDirId, (DriverRef*)&self));
    try(Driver_SetMaxChildCount((DriverRef)self, GP_PORT_COUNT));

    mtx_init(&self->io_mtx);

catch:
    *pOutSelf = self;
    return err;
}

//
// Lifecycle
//

errno_t GamePortController_onStart(GamePortControllerRef _Nonnull _Locked self)
{
    decl_try_err();

    DirEntry be;
    be.dirId = Driver_GetParentDirectoryId(self);
    be.name = "gp-bus";
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
    try(GamePortController_SetPortDevice_Locked(self, 0, IOGP_MOUSE));
    
catch:
    if (err != EOK) {
        Driver_Unpublish(self);
        DriverManager_RemoveDirectory(gDriverManager, self->busDirId);
    }
    return err;
}

void GamePortController_onStop(DriverRef _Nonnull _Locked self)
{
    Driver_Unpublish(self);
}


//
// API
//

static errno_t GamePortController_GetPortDevice(GamePortControllerRef _Nonnull self, int port, int* _Nonnull pOutType)
{
    if (port < 0 || port >= GP_PORT_COUNT) {
        return EINVAL;
    }

    mtx_lock(&self->io_mtx);
    *pOutType = Driver_GetChildDataAt((DriverRef)self, port);
    mtx_unlock(&self->io_mtx);

    return EOK;
}

static errno_t GamePortController_SetPortDevice(GamePortControllerRef _Nonnull self, int port, int type)
{
    mtx_lock(&self->io_mtx);
    const errno_t err = GamePortController_SetPortDevice_Locked(self, port, type);
    mtx_unlock(&self->io_mtx);

    return err;
}

errno_t GamePortController_ioctl(GamePortControllerRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kGamePortCommand_GetPortDevice: {
            const int port = va_arg(ap, int);
            int* itype = va_arg(ap, int*);

            return GamePortController_GetPortDevice(self, port, itype);
        }

        case kGamePortCommand_SetPortDevice: {
            const int port = va_arg(ap, int);
            const int itype = va_arg(ap, int);

            return GamePortController_SetPortDevice(self, port, itype);
        }

        default:
            return super_n(ioctl, Handler, GamePortController, self, pChannel, cmd, ap);
    }
}


//
// Private
//

errno_t GamePortController_createInputDriver(GamePortControllerRef _Nonnull self, int port, int type, DriverRef _Nullable * _Nonnull pOutDriver)
{
    switch (type) {
        case IOGP_MOUSE:
            return MouseDriver_Create(self->busDirId, port, pOutDriver);

        case IOGP_LIGHTPEN:
            return LightPenDriver_Create(self->busDirId, port, pOutDriver);

        case IOGP_ANALOG_JOYSTICK:
            return AnalogJoystickDriver_Create(self->busDirId, port, pOutDriver);

        case IOGP_DIGITAL_JOYSTICK:
            return DigitalJoystickDriver_Create(self->busDirId, port, pOutDriver);

        default:
            return EINVAL;
    }
}

static errno_t GamePortController_SetPortDevice_Locked(GamePortControllerRef _Nonnull _Locked self, int port, int type)
{
    decl_try_err();

    if (port < 0 || port >= GP_PORT_COUNT) {
        return EINVAL;
    }

    switch (type) {
        case IOGP_NONE:
        case IOGP_MOUSE:
        case IOGP_LIGHTPEN:
        case IOGP_ANALOG_JOYSTICK:
        case IOGP_DIGITAL_JOYSTICK:
            break;

        default:
            return EINVAL;
    }


    Driver_StopChildAt((DriverRef)self, port, kDriverStop_Shutdown);
    Driver_SetChildDataAt((DriverRef)self, port, type);

    if (type != IOGP_NONE) {
        DriverRef newDriver = NULL;

        try(GamePortController_CreateInputDriver(self, port, type, &newDriver));
        try(Driver_StartAdoptChildAt((DriverRef)self, port, newDriver));
        Driver_SetChildDataAt((DriverRef)self, port, type);
    }

catch:
    return err;
}


class_func_defs(GamePortController, Driver,
override_func_def(onStart, GamePortController, Driver)
override_func_def(onStop, GamePortController, Driver)
override_func_def(ioctl, GamePortController, Handler)
func_def(createInputDriver, GamePortController)
);
