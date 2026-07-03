//
//  IOGPBus.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#if __IOGPBUS__ > 0

#include "IOGPBus.h"
#include <driver/IORegistry.h>
#include <kpi/hid.h>

IOCATS_DEF(g_cats, IOBUS_GP);


static errno_t IOGPBus_SetPortDevice_Locked(IOGPBusRef _Nonnull _Locked self, int port, int type);


errno_t IOGPBus_Create(Class* _Nonnull pClass, IOGPBusRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    IOGPBusRef self;

    try(IODriver_Create(pClass, g_cats, (IODriverRef*)&self));

    mtx_init(&self->mtx);

    for (int i = 0; i < __IOGPBUS__; i++) {
        self->portType[i] = HID_PORT_NONE;
    }

catch:
    *pOutSelf = self;
    return err;
}

//
// Lifecycle
//

void IOGPBus_onLaunched(IOGPBusRef _Nonnull self)
{
    IOGPBus_SetPortDevice_Locked(self, 0, HID_PORT_MOUSE);
}

bool IOGPBus_isExclusive(IOGPBusRef _Nonnull self)
{
    return false;
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
        *pOutType = self->portType[port];
    }
    if (pOutId) {
        *pOutId = self->portDriverId[port];
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
        if (self->portDriverId[i] == id) {
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

errno_t IOGPBus_createPortDriver(IOGPBusRef _Nonnull self, int port, int type, IODriverRef _Nullable * _Nonnull pOutDriver)
{
    return EINVAL;
}

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


    IODriverRef oldDriver = IORegistry_CopyDriverWithId(gIORegistry, self->portDriverId[port]);
    if (oldDriver) {
        IODriver_Terminate(oldDriver);
        Object_Release(oldDriver);

        self->portDriverId[port] = 0;
        self->portType[port] = HID_PORT_NONE;
    }


    if (type != HID_PORT_NONE) {
        IODriverRef newDriver;

        err = IOGPBus_CreatePortDriver(self, port, type, &newDriver);
        if (err == EOK) {
            err = IODriver_Launch(newDriver, (IODriverRef)self);
            if (err == EOK) {
                self->portDriverId[port] = IODriver_GetId(newDriver);
                self->portType[port] = type;
            }
            Object_Release(newDriver);
        }
    }

    return err;
}


class_func_defs(IOGPBus, IODriver,
override_func_def(onLaunched, IOGPBus, IODriver)
override_func_def(isExclusive, IOGPBus, IODriver)
func_def(createPortDriver, IOGPBus)
);

#endif
