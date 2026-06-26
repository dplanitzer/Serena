//
//  IOGPBus.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#if __IOGPBUS__ > 0

#include "IOGPBus.h"
#include <kpi/hid.h>

IOCATS_DEF(g_cats, IOBUS_GP);


static errno_t IOGPBus_SetPortDevice_Locked(IOGPBusRef _Nonnull _Locked self, int port, int type);


errno_t IOGPBus_Create(IOGPCreateDriverFunc _Nonnull func, void* _Nullable ctx, IOGPBusRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    IOGPBusRef self;

    try(Driver_Create(class(IOGPBus), kDriver_IsBus | kDriver_Exclusive, g_cats, (DriverRef*)&self));
    try(Driver_SetMaxChildCount((DriverRef)self, __IOGPBUS__));

    mtx_init(&self->mtx);
    self->createHidDevice = func;
    self->ctx = ctx;

catch:
    *pOutSelf = self;
    return err;
}

//
// Lifecycle
//

errno_t IOGPBus_onStart(IOGPBusRef _Nonnull _Locked self)
{
    return IOGPBus_SetPortDevice_Locked(self, 0, HID_PORT_MOUSE);
}



//
// API
//

size_t IOGPBus_GetPortCount(IOGPBusRef _Nonnull self)
{
    return __IOGPBUS__;
}

errno_t IOGPBus_GetPortDevice(IOGPBusRef _Nonnull self, int port, int* _Nullable pOutType, did_t* _Nullable pOutId)
{
    if (port < 0 || port >= __IOGPBUS__) {
        return EINVAL;
    }

    mtx_lock(&self->mtx);
    if (pOutType) {
        *pOutType = Driver_GetChildDataAt((DriverRef)self, port);
    }
    if (pOutId) {
        DriverRef pd = Driver_GetChildAt((DriverRef)self, port);

        *pOutId = (pd) ? Driver_GetId(pd) : 0;
    }
    mtx_unlock(&self->mtx);

    return EOK;
}

errno_t IOGPBus_SetPortDevice(IOGPBusRef _Nonnull self, int port, int type)
{
    mtx_lock(&self->mtx);
    const errno_t err = IOGPBus_SetPortDevice_Locked(self, port, type);
    mtx_unlock(&self->mtx);

    return err;
}

errno_t IOGPBus_GetPortForDeviceId(IOGPBusRef _Nonnull self, did_t id, int* _Nonnull pOutPort)
{
    int port = -1;

    mtx_lock(&self->mtx);
    for (int i = 0; i < __IOGPBUS__; i++) {
        DriverRef dp = Driver_GetChildAt((DriverRef)self, i);

        if (dp && Driver_GetId(dp) == id) {
            port = i;
            break;
        }
    }

    *pOutPort = port;
    mtx_unlock(&self->mtx);
    
    return EOK;
}


//
// Private
//

static errno_t IOGPBus_SetPortDevice_Locked(IOGPBusRef _Nonnull _Locked self, int port, int type)
{
    decl_try_err();

    if (port < 0 || port >= __IOGPBUS__) {
        return EINVAL;
    }

    switch (type) {
        case HID_PORT_NONE:
        case HID_PORT_MOUSE:
        case HID_PORT_LIGHT_PEN:
        case HID_PORT_PADDLE:
        case HID_PORT_JOYSTICK:
            break;

        default:
            return EINVAL;
    }


    Driver_DetachChild((DriverRef)self, kDriverStop_Shutdown, port);
    Driver_SetChildDataAt((DriverRef)self, port, HID_PORT_NONE);


    if (type != HID_PORT_NONE) {
        DriverRef newDriver = NULL;

        err = self->createHidDevice(self->ctx, port, type, &newDriver);
        if (err == EOK) {
            err = Driver_AttachStartChild((DriverRef)self, newDriver, port);
            if (err == EOK) {
                Driver_SetChildDataAt((DriverRef)self, port, type);
            }
            Object_Release(newDriver);
        }
    }

    return err;
}


class_func_defs(IOGPBus, Driver,
override_func_def(onStart, IOGPBus, Driver)
);

#endif
