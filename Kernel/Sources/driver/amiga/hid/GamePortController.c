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
#include <System/HID.h>

#define PORT_COUNT   2


final_class_ivars(GamePortController, Driver,
);


errno_t GamePortController_Create(DriverRef _Nullable parent, GamePortControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(GamePortController, kDriver_Exclusive, parent, (DriverRef*)pOutSelf);
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
            return MouseDriver_Create((DriverRef)self, port, pOutDriver);

        case kInputType_DigitalJoystick:
            return DigitalJoystickDriver_Create((DriverRef)self, port, pOutDriver);

        case kInputType_AnalogJoystick:
            return AnalogJoystickDriver_Create((DriverRef)self, port, pOutDriver);

        case kInputType_LightPen:
            return LightPenDriver_Create((DriverRef)self, port, pOutDriver);

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

    if ((err = Driver_Publish((DriverRef)self, "gp-bus", 0)) == EOK) {
        err = GamePortController_SetPortDevice_Locked(self, 0, kInputType_Mouse);
        if (err != EOK) {
            Driver_Unpublish((DriverRef)self);
        }
    }
    
    return err;
}

errno_t GamePortController_ioctl(GamePortControllerRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kGamePortCommand_GetPortDevice:
            return GamePortController_GetPortDevice(self, va_arg(ap, int), va_arg(ap, InputType*));

        case kGamePortCommand_SetPortDevice:
            return GamePortController_SetPortDevice(self, va_arg(ap, int), va_arg(ap, InputType));

        default:
            return super_n(ioctl, Driver, GamePortController, self, cmd, ap);
    }
}


class_func_defs(GamePortController, Driver,
override_func_def(onStart, GamePortController, Driver)
override_func_def(ioctl, GamePortController, Driver)
);
