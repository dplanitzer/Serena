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
#include <kern/assert.h>
#include <kpi/fcntl.h>
#include <kpi/hid.h>


#define PORT_COUNT   2


final_class_ivars(GamePortController, Driver,
    CatalogId   busDirId;
);


errno_t GamePortController_Create(CatalogId parentDirId, GamePortControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(GamePortController), kDriver_Exclusive, NULL, parentDirId, (DriverRef*)pOutSelf);
}

static errno_t GamePortController_GetPortDevice(GamePortControllerRef _Nonnull self, int port, InputType* _Nullable pOutType)
{
    DriverRef dp;

    if (pOutType == NULL) {
        return EINVAL;
    }

    if (port < 0 || port >= PORT_COUNT) {
        return EINVAL;
    }

    dp = Driver_GetChildWithTag((DriverRef)self, port);
    *pOutType = (dp) ? InputDriver_GetInputType(dp) : kInputType_None;

    return EOK;
}

static errno_t GamePortController_CreateInputDriver(GamePortControllerRef _Nonnull self, int port, InputType type, DriverRef _Nullable * _Nonnull pOutDriver)
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
            abort();
    }
}

static errno_t GamePortController_SetPortDevice_Locked(GamePortControllerRef _Nonnull _Locked self, int port, InputType type)
{
    decl_try_err();
    DriverRef pOldDriver = NULL;
    DriverRef pNewDriver = NULL;

    if (port < 0 || port >= PORT_COUNT) {
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


    pOldDriver = Driver_GetChildWithTag((DriverRef)self, port);
    if (pOldDriver) {
        Driver_Terminate((DriverRef)pOldDriver);
        Driver_RemoveChild((DriverRef)self, pOldDriver);
    }

    if (type != kInputType_None) {
        try(GamePortController_CreateInputDriver(self, port, type, &pNewDriver));
        try(Driver_StartAdoptChild((DriverRef)self, pNewDriver));
    }

catch:
    return err;
}

static errno_t GamePortController_SetPortDevice(GamePortControllerRef _Nonnull self, int port, InputType type)
{
    Driver_Lock(self);
    const errno_t err = GamePortController_SetPortDevice_Locked(self, port, type);
    Driver_Unlock(self);

    return err;
}


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
    ((DriverRef)self)->busCatalogId = self->busDirId;

    DriverEntry1 de;
    de.dirId = self->busDirId;
    de.name = "self";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.arg = 0;

    try(DriverManager_Publish(gDriverManager, (DriverRef)self, &de));
    try(GamePortController_SetPortDevice_Locked(self, 0, kInputType_Mouse));
    
catch:
    if (err != EOK) {
        DriverManager_Unpublish(gDriverManager, (DriverRef)self);
        DriverManager_RemoveDirectory(gDriverManager, self->busDirId);
    }
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
            return super_n(ioctl, Driver, GamePortController, self, pChannel, cmd, ap);
    }
}


class_func_defs(GamePortController, Driver,
override_func_def(onStart, GamePortController, Driver)
override_func_def(ioctl, GamePortController, Driver)
);
