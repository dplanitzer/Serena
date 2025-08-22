//
//  Driver.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Driver.h"
#include "DriverManager.h"
#include <driver/DriverChannel.h>
#include <kern/kalloc.h>
#include <kpi/fcntl.h>

typedef struct drv_child {
    DriverRef _Nullable driver;
    intptr_t            data;
} drv_child_t;


errno_t Driver_Create(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverRef self = NULL;

    err = Handler_Create(pClass, (HandlerRef*)&self);
    if (err == EOK) {
        mtx_init(&self->mtx);
        mtx_init(&self->childMtx);

        self->cats = cats;
        self->options = options;
        self->state = kDriverState_Inactive;
    }

    *pOutSelf = self;
    return err;
}

errno_t Driver_CreateRoot(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverRef self = NULL;

    err = Driver_Create(pClass, options, cats, &self);
    if (err == EOK) {
        self->flags |= (kDriverFlag_IsRoot | kDriverFlag_IsAttached); 
    }

    *pOutSelf = self;
    return err;
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
            }
            else {
                err = ENOTATTACHED;
            }
            break;
    }
    mtx_unlock(&self->mtx);

    if (hasStarted) {
        DriverManager_OnDriverStarted(gDriverManager, self);
    }

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

    DriverManager_OnDriverStopping(gDriverManager, self);

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
    Driver_Unpublish(self);
    Driver_UnpublishBusDirectory(self);
    
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


errno_t Driver_publish(DriverRef _Nonnull self, const DriverEntry* _Nonnull de)
{
    return DriverManager_Publish(gDriverManager, de, &self->id);
}

void Driver_unpublish(DriverRef _Nonnull self)
{
    if (self->id > 0) {
        DriverManager_Unpublish(gDriverManager, self->id);
        self->id = 0;
    }
}

errno_t Driver_PublishBusDirectory(DriverRef _Nonnull self, const DirEntry* _Nonnull de)
{
    return DriverManager_CreateDirectory(gDriverManager, de, &self->ownedBusDirId);
}

void Driver_UnpublishBusDirectory(DriverRef _Nonnull self)
{
    if (self->ownedBusDirId > 0) {
        DriverManager_RemoveDirectory(gDriverManager, self->ownedBusDirId);
        self->ownedBusDirId = 0;
    }
}

CatalogId Driver_GetBusDirectory(DriverRef _Nonnull self)
{
    DriverRef bdp = Driver_GetBusController(self);

    return (bdp) ? bdp->ownedBusDirId : 0;
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
    return DriverChannel_Create(self, SEO_FT_DRIVER, mode, 0, pOutChannel);
}

errno_t Driver_open(DriverRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();

    mtx_lock(&self->mtx);

    if (!Driver_IsActive(self)) {
        throw(ENODEV);
    }
    if ((self->options & kDriver_Exclusive) == kDriver_Exclusive && self->openCount > 0) {
        throw(EBUSY);
    }


    try(Driver_OnOpen(self, self->openCount, mode, arg, pOutChannel));
    self->openCount++;

catch:
    mtx_unlock(&self->mtx);

    if (err != EOK) {
        *pOutChannel = NULL;
    }

    return err;
}


void Driver_onClose(DriverRef _Nonnull _Locked self, IOChannelRef _Nonnull pChannel, int openCount)
{
}

errno_t Driver_close(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel)
{
    decl_try_err();

    mtx_lock(&self->mtx);

    if (self->openCount <= 0) {
        throw(EBADF);
    }


    Driver_OnClose(self, pChannel, self->openCount);
    self->openCount--;

catch:
    mtx_unlock(&self->mtx);

    return err;
}

errno_t Driver_ioctl(DriverRef _Nonnull self, IOChannelRef _Nonnull ioc, int cmd, va_list ap)
{
    switch (cmd) {
        case kDriverCommand_GetId: {
            did_t* pdid = va_arg(ap, did_t*);

            *pdid = self->id;
            return EOK;
        }

        case kDriverCommand_GetCategories: {
            iocat_t* buf = va_arg(ap, iocat_t*);
            const size_t bufsiz = va_arg(ap, size_t);
            const iocat_t* sp = self->cats;
            size_t i, len = 0;

            if (bufsiz == 0) {
                return EINVAL;
            }

            while (*sp != IOCAT_END) {
                sp++; len++;
            }

            if (bufsiz < len + 1) {
                return ERANGE;
            }

            for (i = 0; i < __min(len, bufsiz - 1); i++) {
                buf[i] = self->cats[i];
            }
            buf[i] = IOCAT_END;

            return EOK;
        }

        default:
            return ENOTIOCTLCMD;
    }
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

DriverRef _Nullable Driver_GetBusController(DriverRef _Nonnull self)
{
    DriverRef cp = self->parent;

    while (cp) {
        if ((cp->options & kDriver_IsBus) == kDriver_IsBus) {
            return cp;
        }

        cp = cp->parent;
    }

    return NULL;
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
        if (self->child[i].driver) {
            throw(EBUSY);
        }
    }
    

    // Free the old bus storage
    kfree(self->child);
    self->child = NULL;
    self->maxChildCount = 0;
    

    // Allocate the new bus storage, if needed
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

size_t Driver_GetChildCount(DriverRef _Nonnull self)
{
    size_t count = 0;

    mtx_lock(&self->childMtx);
    for (int16_t i = 0; i < self->maxChildCount; i++) {
        if (self->child[i].driver) {
            count++;
        }
    }
    mtx_unlock(&self->childMtx);

    return count;
}

bool Driver_HasChildren(DriverRef _Nonnull self)
{
    return Driver_GetFirstAvailableSlotId(self) != -1;
}


ssize_t Driver_GetFirstAvailableSlotId(DriverRef _Nonnull self)
{
    ssize_t idx = -1;

    mtx_lock(&self->childMtx);
    for (int16_t i = 0; i < self->maxChildCount; i++) {
        if (self->child[i].driver == NULL) {
            idx = i;
            break;
        }
    }
    mtx_unlock(&self->childMtx);

    return idx;
}


DriverRef _Nullable Driver_GetChildAt(DriverRef _Nonnull self, size_t slotId)
{
    mtx_lock(&self->childMtx);
    DriverRef dp = (slotId < self->maxChildCount) ? self->child[slotId].driver : NULL;
    mtx_unlock(&self->childMtx);

    return dp;
}

DriverRef _Nullable Driver_CopyChildAt(DriverRef _Nonnull self, size_t slotId)
{
    mtx_lock(&self->childMtx);
    DriverRef dp = (slotId < self->maxChildCount) ? Object_RetainAs(self->child[slotId].driver, Driver) : NULL;
    mtx_unlock(&self->childMtx);

    return dp;
}


void Driver_DetachChild(DriverRef _Nonnull self, int stopReason, size_t slotId)
{
    DriverRef child = Driver_CopyChildAt(self, slotId);

    if (child) {
        Driver_Stop(self, stopReason);
        Driver_WaitForStopped(child);

        Driver_OnDetaching(child, self);
        Object_Release(child);  // Release the tmp strong ref from CopyChildAt()

        mtx_lock(&self->childMtx);
        Object_Release(self->child[slotId].driver); // Release the slot ref
        self->child[slotId].driver = NULL;
        mtx_unlock(&self->childMtx);
    }
}

errno_t Driver_AttachChild(DriverRef _Nonnull self, DriverRef _Nonnull child, size_t slotId)
{
    decl_try_err();

    if ((child->flags & kDriverFlag_IsAttached) == kDriverFlag_IsAttached) {
        return EBUSY;
    }


    mtx_lock(&self->childMtx);
    if (slotId >= self->maxChildCount) {
        err = ERANGE;
    }
    else if (self->child[slotId].driver) {
        err = EBUSY;
    }
    else {
        self->child[slotId].driver = Object_RetainAs(child, Driver);
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
            Driver_DetachChild(self, kDriverStop_Shutdown, slotId);
        }
    }

    return err;
}


intptr_t Driver_GetChildDataAt(DriverRef _Nonnull self, size_t slotId)
{
    intptr_t data = 0;

    mtx_lock(&self->childMtx);
    if (slotId < self->maxChildCount) {
        data = self->child[slotId].data;
    }
    mtx_unlock(&self->childMtx);

    return data;
}

intptr_t Driver_SetChildDataAt(DriverRef _Nonnull self, size_t slotId, intptr_t data)
{
    intptr_t oldData = 0;

    mtx_lock(&self->childMtx);
    if (slotId < self->maxChildCount) {
        oldData = self->child[slotId].data;
        self->child[slotId].data = data;
    }
    mtx_unlock(&self->childMtx);

    return oldData;
}


class_func_defs(Driver, Handler,
override_func_def(deinit, Driver, Object)
func_def(onStart, Driver)
func_def(onStop, Driver)
func_def(onWaitForStopped, Driver)
func_def(onAttached, Driver)
func_def(onDetaching, Driver)
func_def(publish, Driver)
func_def(unpublish, Driver)
func_def(open, Driver)
func_def(onOpen, Driver)
func_def(close, Driver)
func_def(onClose, Driver)
override_func_def(ioctl, Driver, Handler)
);
