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


static errno_t GamePortController_SetPortDevice_Locked(GamePortControllerRef _Nonnull _Locked self, int port, InputType type);


errno_t GamePortController_Create(CatalogId parentDirId, GamePortControllerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    GamePortControllerRef self;

    err = Driver_Create(class(GamePortController), kDriver_Exclusive, parentDirId, (DriverRef*)&self);
    if (err == EOK) {
        mtx_init(&self->io_mtx);
    }

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
    try(GamePortController_SetPortDevice_Locked(self, 0, kInputType_Mouse));
    
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

static errno_t GamePortController_GetPortDevice(GamePortControllerRef _Nonnull self, int port, InputType* _Nonnull pOutType)
{
    if (port < 0 || port >= GP_PORT_COUNT) {
        return EINVAL;
    }

    mtx_lock(&self->io_mtx);
    *pOutType = (self->portDriver[port]) ? InputDriver_GetInputType(self->portDriver[port]) : kInputType_None;
    mtx_unlock(&self->io_mtx);

    return EOK;
}

static errno_t GamePortController_SetPortDevice(GamePortControllerRef _Nonnull self, int port, InputType type)
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
            InputType* itype = va_arg(ap, InputType*);

            return GamePortController_GetPortDevice(self, port, itype);
        }

        case kGamePortCommand_SetPortDevice: {
            const int port = va_arg(ap, int);
            InputType itype = va_arg(ap, InputType);

            return GamePortController_SetPortDevice(self, port, itype);
        }

        default:
            return super_n(ioctl, Handler, GamePortController, self, pChannel, cmd, ap);
    }
}


//
// Private
//

errno_t GamePortController_createInputDriver(GamePortControllerRef _Nonnull self, int port, InputType type, DriverRef _Nullable * _Nonnull pOutDriver)
{
    switch (type) {
        case kInputType_Mouse:
            return MouseDriver_Create(self->busDirId, port, pOutDriver);

        case kInputType_DigitalJoystick:
            return DigitalJoystickDriver_Create(self->busDirId, port, pOutDriver);

        case kInputType_AnalogJoystick:
            return AnalogJoystickDriver_Create(self->busDirId, port, pOutDriver);

        case kInputType_LightPen:
            return LightPenDriver_Create(self->busDirId, port, pOutDriver);

        default:
            return EINVAL;
    }
}

static errno_t GamePortController_SetPortDevice_Locked(GamePortControllerRef _Nonnull _Locked self, int port, InputType type)
{
    decl_try_err();

    if (port < 0 || port >= GP_PORT_COUNT) {
        return EINVAL;
    }

    switch (type) {
        case kInputType_Mouse:
        case kInputType_DigitalJoystick:
        case kInputType_AnalogJoystick:
        case kInputType_LightPen:
            break;

        default:
            return EINVAL;
    }


    if (self->portDriver[port]) {
        Driver_Stop(self->portDriver[port], kDriverStop_Shutdown);
        Driver_RemoveChild((DriverRef)self, self->portDriver[port]);
        self->portDriver[port] = NULL;
    }

    if (type != kInputType_None) {
        try(GamePortController_CreateInputDriver(self, port, type, &self->portDriver[port]));
        try(Driver_StartAdoptChild((DriverRef)self, self->portDriver[port]));
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
