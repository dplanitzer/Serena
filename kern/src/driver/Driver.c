//
//  Driver.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Driver.h"
#include "IORegistry.h"
#include <assert.h>
#include <ext/atomic.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <kpi/file.h>


static volatile atomic_int   g_next_driver_id = {.value = 1};

errno_t Driver_Create(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverRef self;

    err = Object_Create(pClass, 0, (void**)&self);
    if (err == EOK) {
        mtx_init(&self->mtx);

        self->id = atomic_int_fetch_add(&g_next_driver_id, 1);
        self->cats = cats;
        self->options = options;
        self->state = kDriverState_Inactive;

        *pOutSelf = self;
    }

    return err;
}


void Driver_deinit(DriverRef _Nonnull self)
{
    switch (self->state) {
        case kDriverState_Inactive:
        case kDriverState_Stopped:
            break;

        default:
            abort();
            /* NOT REACHED */
    }

    mtx_deinit(&self->mtx);
}


//
// MARK: -
// MARK: API
//

errno_t Driver_Start(DriverRef _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    switch (self->state) {
        case kDriverState_Active:
            err = EBUSY;
            break;

        case kDriverState_Stopping:
        case kDriverState_Stopped:
            err = ENODEV;
            break;
            
        default:
            self->state = kDriverState_Active;
            err = Driver_OnStart(self);
            break;
    }
    mtx_unlock(&self->mtx);

    return err;
}

errno_t Driver_onStart(DriverRef _Nonnull _Locked self)
{
    return EOK;
}


void Driver_Stop(DriverRef _Nonnull self)
{
    bool doStop = true;

    // Validate our state
    mtx_lock(&self->mtx);
    switch (self->state) {
        case kDriverState_Stopping:
        case kDriverState_Stopped:
            doStop = false;
            break;

        default:
            break;
    }
    mtx_unlock(&self->mtx);

    if (!doStop) {
        return;
    }


    // Enter the stopping state
    mtx_lock(&self->mtx);
    Driver_Unpublish(self);    
    Driver_OnStop(self);
    self->state = kDriverState_Stopping;
    mtx_unlock(&self->mtx);
}

void Driver_onStop(DriverRef _Nonnull _Locked self)
{
}


void Driver_WaitForStopped(DriverRef _Nonnull self)
{
    // Let the driver subclass wait for the shutdown of whatever asynchronous
    // processes it depends on
    Driver_OnWaitForStopped(self);


    // Change the state to stopped
    mtx_lock(&self->mtx);
    self->state = kDriverState_Stopped;
    mtx_unlock(&self->mtx);
}

void Driver_onWaitForStopped(DriverRef _Nonnull self)
{
}


void Driver_doRegister(DriverRef _Nonnull self)
{
    IORegistry_RegisterDriver(gIORegistry, self);
}

void Driver_doDeregister(DriverRef _Nonnull self)
{
    IORegistry_DeregisterDriver(gIORegistry, self);
}


void Driver_attachProvider(DriverRef _Nonnull self, DriverRef _Nonnull provider)
{
    IORegistry_AttachProvider(gIORegistry, provider, self);
}

void Driver_detachProvider(DriverRef _Nonnull self, DriverRef _Nonnull provider)
{
    IORegistry_DetachProvider(gIORegistry, self);
}


errno_t Driver_Publish(DriverRef _Nonnull self, const devfs_entry_t* _Nonnull en)
{
    if (self->devfs_hnd > 0) {
        return EBUSY;
    }

    return devfs_add(en, &self->devfs_hnd);
}

void Driver_Unpublish(DriverRef _Nonnull self)
{
    if (self->devfs_hnd > 0) {
        devfs_remove(self->devfs_hnd);
        self->devfs_hnd = 0;
    }
}


errno_t Driver_Launch(DriverRef _Nonnull client, DriverRef _Nullable provider)
{
    decl_try_err();

    if (provider) {
        Driver_AttachProvider(client, provider);
    }

    err = Driver_Start(client);
    if (err == EOK) {
        Driver_Register(client);
    }

    return err;
}

void Driver_TerminateClients(DriverRef _Nonnull self)
{
    decl_try_err();
    IOIterator iter;

    err = IORegistry_CopyClientDrivers(gIORegistry, self, &iter);
    if (err == EOK) {
        while (IOIterator_HasNext(&iter)) {
            Driver_Terminate(IOIterator_GetNext(&iter));
        }
        IOIterator_Destroy(&iter);
    }
}

void Driver_Terminate(DriverRef _Nonnull self)
{
    Driver_TerminateClients(self);

    Driver_Deregister(self);

    Driver_Stop(self);
    Driver_WaitForStopped(self);

    Driver_DetachProvider(self, self->provider);
}


bool Driver_IsOpen(DriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const bool r = (self->openCount > 0) ? true : false;
    mtx_unlock(&self->mtx);

    return r;
}

errno_t Driver_onOpen(DriverRef _Nonnull _Locked self, int openCount, fd_flags_t flags)
{
    return EOK;
}

errno_t Driver_open(DriverRef _Nonnull self, fd_flags_t flags)
{
    decl_try_err();

    mtx_lock(&self->mtx);

    if (!Driver_IsActive(self)) {
        throw(ENODEV);
    }
    if ((self->options & kDriver_Exclusive) == kDriver_Exclusive && self->openCount > 0) {
        throw(EBUSY);
    }


    try(Driver_OnOpen(self, self->openCount, flags));
    self->openCount++;

catch:
    mtx_unlock(&self->mtx);

    return err;
}


void Driver_onClose(DriverRef _Nonnull _Locked self, int openCount)
{
}

void Driver_close(DriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    assert(self->openCount > 0);

    Driver_OnClose(self, self->openCount);
    self->openCount--;
    mtx_unlock(&self->mtx);
}


bool Driver_HasCategory(DriverRef _Nonnull self, iocat_t cat)
{
    // Category info is constant - no need to take a lock here
    const iocat_t* p = self->cats;

    while (*p != cat && *p != IOCAT_END) {
        p++;
    }

    return (*p == cat) ? true : false;
}

bool Driver_HasSomeCategories(DriverRef _Nonnull self, const iocat_t* _Nonnull cats)
{
    while (*cats != IOCAT_END) {
        if (Driver_HasCategory(self, *cats)) {
            return true;
        }

        cats++;
    }

    return false;
}


class_func_defs(Driver, Object,
override_func_def(deinit, Driver, Object)
func_def(onStart, Driver)
func_def(onStop, Driver)
func_def(onWaitForStopped, Driver)
func_def(doRegister, Driver)
func_def(doDeregister, Driver)
func_def(attachProvider, Driver)
func_def(detachProvider, Driver)
func_def(open, Driver)
func_def(onOpen, Driver)
func_def(close, Driver)
func_def(onClose, Driver)
);
