//
//  IODriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "IODriver.h"
#include "IORegistry.h"
#include <assert.h>
#include <ext/atomic.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <kpi/file.h>


static volatile atomic_int   g_next_driver_id = {.value = 1};

errno_t IODriver_Create(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, IODriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    IODriverRef self;

    err = Object_Create(pClass, 0, (void**)&self);
    if (err == EOK) {
        mtx_init(&self->mtx);

        self->id = atomic_int_fetch_add(&g_next_driver_id, 1);
        self->cats = cats;
        self->options = options;

        *pOutSelf = self;
    }

    return err;
}


void IODriver_deinit(IODriverRef _Nonnull self)
{
    if (IODriver_IsActive(self)) {
        abort();
        /* NOT REACHED */
    }

    mtx_deinit(&self->mtx);
}


//
// MARK: -
// MARK: API
//

errno_t IODriver_start(IODriverRef _Nonnull _Locked self)
{
    return EOK;
}


void IODriver_stop(IODriverRef _Nonnull _Locked self)
{
}


void IODriver_doRegister(IODriverRef _Nonnull self)
{
    IORegistry_RegisterDriver(gIORegistry, self);
}

void IODriver_doDeregister(IODriverRef _Nonnull self)
{
    IORegistry_DeregisterDriver(gIORegistry, self);
}


void IODriver_attachProvider(IODriverRef _Nonnull self, IODriverRef _Nonnull provider)
{
    IORegistry_AttachProvider(gIORegistry, provider, self);
}

void IODriver_detachProvider(IODriverRef _Nonnull self, IODriverRef _Nonnull provider)
{
    IORegistry_DetachProvider(gIORegistry, self);
}


errno_t IODriver_Publish(IODriverRef _Nonnull self, const devfs_entry_t* _Nonnull en)
{
    if (self->devfs_hnd > 0) {
        return EBUSY;
    }

    return devfs_add(en, &self->devfs_hnd);
}

void IODriver_Unpublish(IODriverRef _Nonnull self)
{
    if (self->devfs_hnd > 0) {
        devfs_remove(self->devfs_hnd);
        self->devfs_hnd = 0;
    }
}

errno_t IODriver_Launch(IODriverRef _Nonnull self, IODriverRef _Nullable provider)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    if (IODriver_IsActive(self)) {
        mtx_unlock(&self->mtx);
        return EOK;
    }


    if (provider) {
        IODriver_AttachProvider(self, provider);
    }


    err = IODriver_Start(self);
    if (err == EOK) {
        self->flags |= kIODriverFlag_IsActive;
    }
    else if (provider) {
        IODriver_DetachProvider(self, provider);
    }

    mtx_unlock(&self->mtx);


    // Register the driver after dropping the lock since registration can trigger
    // callouts to arbitrary code.
    if (err == EOK) {
        IODriver_Register(self);
    }

    return err;
}

void IODriver_TerminateClients(IODriverRef _Nonnull self)
{
    decl_try_err();
    IOIterator iter;

    err = IORegistry_CopyClientDrivers(gIORegistry, self, &iter);
    if (err == EOK) {
        while (IOIterator_HasNext(&iter)) {
            IODriver_Terminate(IOIterator_GetNext(&iter));
        }
        IOIterator_Destroy(&iter);
    }
}

void IODriver_Terminate(IODriverRef _Nonnull self)
{
    // Validate our state
    mtx_lock(&self->mtx);
    const bool doStop = IODriver_IsActive(self);
    mtx_unlock(&self->mtx);

    if (!doStop) {
        return;
    }


    IODriver_TerminateClients(self);
    IODriver_Deregister(self);
    IODriver_Unpublish(self);    


    // Enter the stopped state
    mtx_lock(&self->mtx);
    IODriver_Stop(self);
    IODriver_DetachProvider(self, self->provider);
    self->flags &= ~kIODriverFlag_IsActive;
    mtx_unlock(&self->mtx);
}


bool IODriver_IsOpen(IODriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const bool r = (self->openCount > 0) ? true : false;
    mtx_unlock(&self->mtx);

    return r;
}

errno_t IODriver_onOpen(IODriverRef _Nonnull _Locked self, int openCount, fd_flags_t flags)
{
    return EOK;
}

errno_t IODriver_open(IODriverRef _Nonnull self, fd_flags_t flags)
{
    decl_try_err();

    mtx_lock(&self->mtx);

    if (!IODriver_IsActive(self)) {
        throw(ENODEV);
    }
    if ((self->options & kIODriver_Exclusive) == kIODriver_Exclusive && self->openCount > 0) {
        throw(EBUSY);
    }


    try(IODriver_OnOpen(self, self->openCount, flags));
    self->openCount++;

catch:
    mtx_unlock(&self->mtx);

    return err;
}


void IODriver_onClose(IODriverRef _Nonnull _Locked self, int openCount)
{
}

void IODriver_close(IODriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    assert(self->openCount > 0);

    IODriver_OnClose(self, self->openCount);
    self->openCount--;
    mtx_unlock(&self->mtx);
}


bool IODriver_HasCategory(IODriverRef _Nonnull self, iocat_t cat)
{
    // Category info is constant - no need to take a lock here
    const iocat_t* p = self->cats;

    while (*p != cat && *p != IOCAT_END) {
        p++;
    }

    return (*p == cat) ? true : false;
}

bool IODriver_HasSomeCategories(IODriverRef _Nonnull self, const iocat_t* _Nonnull cats)
{
    while (*cats != IOCAT_END) {
        if (IODriver_HasCategory(self, *cats)) {
            return true;
        }

        cats++;
    }

    return false;
}


class_func_defs(IODriver, Object,
override_func_def(deinit, IODriver, Object)
func_def(start, IODriver)
func_def(stop, IODriver)
func_def(doRegister, IODriver)
func_def(doDeregister, IODriver)
func_def(attachProvider, IODriver)
func_def(detachProvider, IODriver)
func_def(open, IODriver)
func_def(onOpen, IODriver)
func_def(close, IODriver)
func_def(onClose, IODriver)
);
