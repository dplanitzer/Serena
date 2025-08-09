//
//  Driver.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Driver.h"
#include "DriverManager.h"
#include <handler/HandlerChannel.h>
#include <kern/kalloc.h>
#include <kpi/fcntl.h>

typedef struct drv_child {
    DriverRef _Nullable driver;
    intptr_t            data;
} drv_child_t;


errno_t Driver_Create(Class* _Nonnull pClass, unsigned options, CatalogId parentDirectoryId, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverRef self = NULL;

    err = Handler_Create(pClass, (HandlerRef*)&self);
    if (err == EOK) {
        mtx_init(&self->mtx);
        cnd_init(&self->cnd);
        mtx_init(&self->childMtx);

        self->options = options;
        self->state = kDriverState_Inactive;
        self->parentDirectoryId = parentDirectoryId;
    }

    *pOutSelf = self;
    return EOK;
}


void Driver_deinit(DriverRef _Nonnull self)
{
    switch (self->state) {
        case kDriverState_Stopping:
            Driver_WaitForStopped(self);
            break;

        case kDriverState_Inactive:
        case kDriverState_Stopped:
            break;

        case kDriverState_Active:
            abort();
    }


    if (self->maxChildCount > 0) {
        for (int16_t i = 0; i < self->maxChildCount; i++) {
            if (self->child[i].driver) {
                Object_Release(self->child[i].driver);
                self->child[i].driver = NULL;
            }
        }
    }


    cnd_deinit(&self->cnd);
    mtx_deinit(&self->mtx);
    mtx_deinit(&self->childMtx);
}

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
            Driver_OnStart(self);
            break;
    }
    mtx_unlock(&self->mtx);

    return err;
}

errno_t Driver_onStart(DriverRef _Nonnull _Locked self)
{
    return EOK;
}


void Driver_Stop(DriverRef _Nonnull self, int reason)
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


    // The list of child drivers is now frozen and can not change anymore.
    // Tell all our child drivers to stop.
    mtx_lock(&self->childMtx);
    for (int16_t i = 0; i < self->maxChildCount; i++) {
        if (self->child[i].driver) {
            // We temporarily drop the lock so that the child is able to eg remove
            // itself from us. That's fine because the child array has a fixed size
            // and our iterator 'i' is stable. Also no new children can be added
            // anymore anyway.
            mtx_unlock(&self->childMtx);
            Driver_Stop(self->child[i].driver, reason);
            mtx_lock(&self->childMtx);
        }
    }
    mtx_unlock(&self->childMtx);


    // Enter the stopping state
    mtx_lock(&self->mtx);
    Driver_OnStop(self);
    self->state = kDriverState_Stopping;
    mtx_unlock(&self->mtx);


    // Tell the driver manager that we are ready for reaping
    DriverManager_ReapDriver(gDriverManager, self);
}

void Driver_onStop(DriverRef _Nonnull _Locked self)
{
}


void Driver_WaitForStopped(DriverRef _Nonnull self)
{
    // First wait for all I/O channels to be closed
    mtx_lock(&self->mtx);
    for (;;) {
        if (self->state != kDriverState_Stopped) {
            mtx_unlock(&self->mtx);
            return;
        }

        if (self->openCount == 0) {
            break;
        }

        cnd_wait(&self->cnd, &self->mtx);
    }
    mtx_unlock(&self->mtx);


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


errno_t Driver_publish(DriverRef _Nonnull self, const DriverEntry* _Nonnull de)
{
    return DriverManager_Publish(gDriverManager, de, &self->id);
}

void Driver_unpublish(DriverRef _Nonnull self)
{
    DriverManager_Unpublish(gDriverManager, self->id);
    self->id = 0;
}


bool Driver_HasOpenChannels(DriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const bool r = (self->openCount > 0) ? true : false;
    mtx_unlock(&self->mtx);

    return r;
}

errno_t Driver_onOpen(DriverRef _Nonnull _Locked self, int openCount, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return HandlerChannel_Create((HandlerRef)self, SEO_FT_DRIVER, mode, 0, pOutChannel);
}

errno_t Driver_open(DriverRef _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();

    if ((self->options & kDriver_Exclusive) == kDriver_Exclusive && self->openCount > 0) {
        *pOutChannel = NULL;
        return EBUSY;
    }

    err = Driver_OnOpen(self, self->openCount, mode, arg, pOutChannel);    
    if (err == EOK) {
        self->openCount++;
    }
    else {
        *pOutChannel = NULL;
    }

    return err;
}

errno_t Driver_Open(DriverRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    if (Driver_IsActive(self)) {
        err = Handler_Open(self, mode, arg, pOutChannel);
    }
    else {
        err = ENODEV;
    }
    mtx_unlock(&self->mtx);

    return err;
}


void Driver_onClose(DriverRef _Nonnull _Locked self, IOChannelRef _Nonnull pChannel, int openCount)
{
}

errno_t Driver_close(DriverRef _Nonnull _Locked self, IOChannelRef _Nonnull pChannel)
{
    Driver_OnClose(self, pChannel, self->openCount);

    if (self->openCount > 0) {
        self->openCount--;

        if (self->openCount == 0) {
            // Notify Driver_WaitForStopped()
            cnd_broadcast(&self->cnd);
        }
        return EOK;
    }
    else {
        return EBADF;
    }
}

errno_t Driver_Close(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    if (Driver_IsActive(self)) {
        err = Handler_Close(self, pChannel);
    }
    else {
        err = ENODEV;
    }
    mtx_unlock(&self->mtx);

    return err;
}


errno_t Driver_SetMaxChildCount(DriverRef _Nonnull self, size_t count)
{
    decl_try_err();

    mtx_lock(&self->childMtx);
    if (self->maxChildCount > 0) {
        throw(EBUSY);
    }
    if (count > 1024) {
        throw(EINVAL);
    }

    
    if (count > 0) {
        try(kalloc_cleared(sizeof(drv_child_t) * count, (void**)&self->child));
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

errno_t Driver_AddChild(DriverRef _Nonnull self, DriverRef _Nonnull child)
{
    return Driver_AdoptChild(self, Object_RetainAs(child, Driver));
}

errno_t Driver_AdoptChild(DriverRef _Nonnull self, DriverRef _Nonnull _Consuming child)
{
    bool hasSlot = false;

    if ((self->flags & kDriverFlag_NoMoreChildren) == kDriverFlag_NoMoreChildren) {
        return ETERMINATED;
    }

    mtx_lock(&self->childMtx);
    for (int16_t i = 0; i < self->maxChildCount; i++) {
        if (self->child[i].driver == NULL) {
            self->child[i].driver = child;
            hasSlot = true;
            break;
        }
    }
    mtx_unlock(&self->childMtx);

    return (hasSlot) ? EOK : ENXIO;
}

// Starts the given driver instance and adopts the driver instance as a child if
// the start has been successful.
errno_t Driver_StartAdoptChild(DriverRef _Nonnull self, DriverRef _Nonnull _Consuming child)
{
    decl_try_err();

    if ((err = Driver_Start(child)) == EOK) {
        err = Driver_AdoptChild(self, child);
    }
    return err;
}

// Removes the given driver from the receiver. The given driver has to be a child
// of the receiver.
void Driver_RemoveChild(DriverRef _Nonnull self, DriverRef _Nonnull child)
{
    int16_t idx = -1;

    mtx_lock(&self->childMtx);
    for (int16_t i = 0; i < self->maxChildCount; i++) {
        if (self->child[i].driver == child) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        Object_Release(self->child[idx].driver);
        self->child[idx].driver = NULL;
    }
    mtx_unlock(&self->childMtx);
}

DriverRef _Nullable Driver_RemoveChildAt(DriverRef _Nonnull self, size_t slotId)
{
    DriverRef oldChild = NULL;

    mtx_lock(&self->childMtx);
    if (slotId < self->maxChildCount) {
        oldChild = self->child[slotId].driver;
        self->child[slotId].driver = NULL;
    }
    mtx_unlock(&self->childMtx);

    return oldChild;
}

DriverRef _Nullable Driver_SetChildAt(DriverRef _Nonnull self, size_t slotId, DriverRef _Nullable child)
{
    return Driver_AdoptChildAt(self, slotId, (child) ? Object_RetainAs(child, Driver) : NULL);
}

DriverRef _Nullable Driver_AdoptChildAt(DriverRef _Nonnull self, size_t slotId, DriverRef _Nullable _Consuming child)
{
    DriverRef oldChild = NULL;

    mtx_lock(&self->childMtx);
    if (slotId < self->maxChildCount) {
        oldChild = self->child[slotId].driver;
        self->child[slotId].driver = child;
    }
    mtx_unlock(&self->childMtx);

    return oldChild;
}

DriverRef _Nullable Driver_GetChildAt(DriverRef _Nonnull self, size_t slotId)
{
    DriverRef dp = NULL;

    mtx_lock(&self->childMtx);
    if (slotId < self->maxChildCount) {
        dp = self->child[slotId].driver;
    }
    mtx_unlock(&self->childMtx);

    return dp;
}


errno_t Driver_StartAdoptChildAt(DriverRef _Nonnull self, size_t slotId, DriverRef _Nonnull _Consuming child)
{
    decl_try_err();

    err = Driver_Start(child);
    if (err == EOK) {
        Driver_AdoptChildAt(self, slotId, child);
    }

    return err;
}

void Driver_StopChildAt(DriverRef _Nonnull self, size_t slotId, int stopReason)
{
    DriverRef child = Driver_RemoveChildAt(self, slotId);

    if (child) {
        Driver_Stop(self, stopReason);
        Object_Release(child);  // balances the retain() from the addChild()
    }
}


class_func_defs(Driver, Handler,
override_func_def(deinit, Driver, Object)
func_def(onStart, Driver)
func_def(onStop, Driver)
func_def(onWaitForStopped, Driver)
func_def(publish, Driver)
func_def(unpublish, Driver)
override_func_def(open, Driver, Handler)
func_def(onOpen, Driver)
override_func_def(close, Driver, Handler)
func_def(onClose, Driver)
);
