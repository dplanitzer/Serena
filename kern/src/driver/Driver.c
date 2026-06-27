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
        mtx_init(&self->childMtx);

        self->id = atomic_int_fetch_add(&g_next_driver_id, 1);
        self->cats = cats;
        self->options = options;
        self->state = kDriverState_Inactive;

        *pOutSelf = self;
    }

    return err;
}

errno_t Driver_CreateRoot(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverRef self;

    err = Driver_Create(pClass, options, cats, &self);
    if (err == EOK) {
        self->flags |= kDriverFlag_IsAttached;

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


    if (self->maxChildCount > 0) {
        for (int16_t i = 0; i < self->maxChildCount; i++) {
            if (self->child[i]) {
                Object_Release(self->child[i]);
                self->child[i] = NULL;
            }
        }
    }


    mtx_deinit(&self->mtx);
    mtx_deinit(&self->childMtx);
}


//
// MARK: -
// MARK: API
//

errno_t Driver_Start(DriverRef _Nonnull self)
{
    decl_try_err();
    bool hasStarted = false;

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
            if ((self->flags & kDriverFlag_IsAttached) == kDriverFlag_IsAttached) {
                self->state = kDriverState_Active;
                err = Driver_OnStart(self);
                if (err == EOK) {
                    hasStarted = true;
                }
                else {
                    Driver_Unpublish(self);
                }
            }
            else {
                err = ENOTATTACHED;
            }
            break;
    }
    mtx_unlock(&self->mtx);

    if (hasStarted) {
        Driver_Register(self);
    }

    return err;
}

errno_t Driver_onStart(DriverRef _Nonnull _Locked self)
{
    return EOK;
}


void Driver_Stop(DriverRef _Nonnull self)
{
    bool doStop = true;

    // Validate our state and disable accepting new children
    mtx_lock(&self->mtx);
    switch (self->state) {
        case kDriverState_Stopping:
        case kDriverState_Stopped:
            doStop = false;
            break;

        default:
            self->flags |= kDriverFlag_NoMoreChildren;
            break;
    }
    mtx_unlock(&self->mtx);

    if (!doStop) {
        return;
    }

    Driver_Deregister(self);

    // The list of child drivers is now frozen and can not change anymore.
    // Tell all our child drivers to stop.
    mtx_lock(&self->childMtx);
    for (int16_t i = 0; i < self->maxChildCount; i++) {
        if (self->child[i]) {
            // We temporarily drop the lock so that the child is able to eg remove
            // itself from us. That's fine because the child array has a fixed size
            // and our iterator 'i' is stable. Also no new children can be added
            // anymore anyway.
            mtx_unlock(&self->childMtx);
            Driver_Stop(self->child[i]);
            mtx_lock(&self->childMtx);
        }
    }
    mtx_unlock(&self->childMtx);


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


void Driver_onAttached(DriverRef _Nonnull self, DriverRef _Nonnull parent)
{
    self->parent = parent;
    self->flags |= kDriverFlag_IsAttached;
}

void Driver_onDetaching(DriverRef _Nonnull self, DriverRef _Nonnull parent)
{
    self->parent = NULL;
    self->flags &= ~kDriverFlag_IsAttached;
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


//
// MARK: -
// MARK: Subclassers
//

errno_t Driver_SetMaxChildCount(DriverRef _Nonnull self, size_t count)
{
    decl_try_err();

    if (count > 1024) {
        return EINVAL;
    }


    mtx_lock(&self->childMtx);

    // Only allowed to change the bus size if the driver has no children
    for (int i = 0; i < self->maxChildCount; i++) {
        if (self->child[i]) {
            throw(EBUSY);
        }
    }
    

    // Free the old bus storage
    kfree(self->child);
    self->child = NULL;
    self->maxChildCount = 0;
    

    // Allocate the new bus storage, if needed
    if (count > 0) {
        try(kalloc_cleared(sizeof(DriverRef) * count, (void**)&self->child));
        self->maxChildCount = count;
    }

catch:
    mtx_unlock(&self->childMtx);

    return err;
}

size_t Driver_GetMaxChildCount(DriverRef _Nonnull self)
{
    mtx_lock(&self->childMtx);
    const size_t count = self->maxChildCount;
    mtx_unlock(&self->childMtx);

    return count;
}

size_t Driver_GetChildCount(DriverRef _Nonnull self)
{
    size_t count = 0;

    mtx_lock(&self->childMtx);
    for (int16_t i = 0; i < self->maxChildCount; i++) {
        if (self->child[i]) {
            count++;
        }
    }
    mtx_unlock(&self->childMtx);

    return count;
}


DriverRef _Nullable Driver_GetChildAt(DriverRef _Nonnull self, size_t slotId)
{
    mtx_lock(&self->childMtx);
    DriverRef dp = (slotId < self->maxChildCount) ? self->child[slotId] : NULL;
    mtx_unlock(&self->childMtx);

    return dp;
}

DriverRef _Nullable Driver_CopyChildAt(DriverRef _Nonnull self, size_t slotId)
{
    mtx_lock(&self->childMtx);
    DriverRef dp = (slotId < self->maxChildCount && self->child[slotId]) ? Object_Retain(self->child[slotId]) : NULL;
    mtx_unlock(&self->childMtx);

    return dp;
}


ssize_t _get_first_available_slotid(DriverRef _Nonnull _Locked self)
{
    ssize_t idx = -1;

    for (int16_t i = 0; i < self->maxChildCount; i++) {
        if (self->child[i] == NULL) {
            idx = i;
            break;
        }
    }

    return idx;
}

errno_t Driver_AttachChild(DriverRef _Nonnull self, DriverRef _Nonnull child, size_t slotId)
{
    decl_try_err();

    if ((child->flags & kDriverFlag_IsAttached) == kDriverFlag_IsAttached) {
        return EBUSY;
    }


    mtx_lock(&self->childMtx);
    if (slotId == (size_t)-1) {
        slotId = _get_first_available_slotid(self);
    }
    
    if (slotId >= self->maxChildCount) {
        err = ERANGE;
    }
    else if (self->child[slotId]) {
        err = EBUSY;
    }
    else {
        self->child[slotId] = Object_Retain(child);
    }
    mtx_unlock(&self->childMtx);

    if (err == EOK) {
        Driver_OnAttached(child, self);
    }

    return err;
}

errno_t Driver_AttachStartChild(DriverRef _Nonnull self, DriverRef _Nonnull child, size_t slotId)
{
    decl_try_err();

    err = Driver_AttachChild(self, child, slotId);
    if (err == EOK) {
        err = Driver_Start(child);
        if (err != EOK) {
            Driver_DetachChild(self, slotId);
        }
    }

    return err;
}

void Driver_DetachChild(DriverRef _Nonnull self, size_t slotId)
{
    DriverRef child = Driver_CopyChildAt(self, slotId);

    if (child) {
        Driver_Stop(self);
        Driver_WaitForStopped(child);

        Driver_OnDetaching(child, self);
        Object_Release(child);  // Release the tmp strong ref from CopyChildAt()

        mtx_lock(&self->childMtx);
        Object_Release(self->child[slotId]); // Release the slot ref
        self->child[slotId] = NULL;
        mtx_unlock(&self->childMtx);
    }
}


class_func_defs(Driver, Object,
override_func_def(deinit, Driver, Object)
func_def(onStart, Driver)
func_def(onStop, Driver)
func_def(onWaitForStopped, Driver)
func_def(doRegister, Driver)
func_def(doDeregister, Driver)
func_def(onAttached, Driver)
func_def(onDetaching, Driver)
func_def(open, Driver)
func_def(onOpen, Driver)
func_def(close, Driver)
func_def(onClose, Driver)
);
