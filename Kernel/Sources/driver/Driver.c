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
#include <kpi/fcntl.h>


#define driver_from_child_qe(__ptr) \
(DriverRef) (((uint8_t*)__ptr) - offsetof(struct Driver, child_qe))


errno_t Driver_Create(Class* _Nonnull pClass, unsigned options, CatalogId parentDirectoryId, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverRef self = NULL;

    try(Handler_Create(pClass, (HandlerRef*)&self));

    mtx_init(&self->mtx);
    List_Init(&self->children);
    ListNode_Init(&self->child_qe);
    self->options = options;
    self->state = kDriverState_Inactive;
    self->parentDirectoryId = parentDirectoryId;

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

void Driver_deinit(DriverRef _Nonnull self)
{
    List_ForEach(&self->children, struct Driver,
        DriverRef pCurDriver = driver_from_child_qe(pCurNode);

        Object_Release(pCurDriver);
    );
}

errno_t Driver_Start(DriverRef _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    switch (self->state) {
        case kDriverState_Active:
            err = EBUSY;
            break;

        case kDriverState_Terminating:
        case kDriverState_Terminated:
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


errno_t Driver_Terminate(DriverRef _Nonnull self)
{
    decl_try_err();

    // Change the state to terminating. 
    mtx_lock(&self->mtx);
    switch (self->state) {
        case kDriverState_Terminating:
        case kDriverState_Terminated:
            err = ETERMINATED;
            break;

        default:
            if (self->openCount == 0) {
                self->state = kDriverState_Terminating;
            }
            else {
                err = EBUSY;
            }
            break;
    }
    mtx_unlock(&self->mtx);

    if (err != EOK) {
        return err;
    }

    
    // The list of child drivers is now frozen and can not change anymore.
    // Synchronously terminate all our child drivers
    List_ForEach(&self->children, struct Driver,
        err = Driver_Terminate(driver_from_child_qe(pCurNode));
        if (err != EOK) {
            break;
        }
    );


    mtx_lock(&self->mtx);

    if (err != EOK) {
        self->state = kDriverState_Active;
        mtx_unlock(&self->mtx);
        return err;
    }


    // Stop myself
    Driver_OnStop(self);


    // And mark the driver as terminated
    self->state = kDriverState_Terminated;
    mtx_unlock(&self->mtx);

    return EOK;
}

void Driver_onStop(DriverRef _Nonnull _Locked self)
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

errno_t Driver_close(DriverRef _Nonnull _Locked self, IOChannelRef _Nonnull pChannel)
{
    if (self->openCount > 0) {
        self->openCount--;
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

// Adds the given driver as a child to the receiver.
void Driver_AddChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull pChild)
{
    Driver_AdoptChild(self, Object_RetainAs(pChild, Driver));
}

// Adds the given driver to the receiver as a child. Consumes the provided strong
// reference.
void Driver_AdoptChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull _Consuming pChild)
{
    if (Driver_IsActive(self)) {
        List_InsertAfterLast(&self->children, &pChild->child_qe);
    }
}

// Starts the given driver instance and adopts the driver instance as a child if
// the start has been successful.
errno_t Driver_StartAdoptChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull _Consuming pChild)
{
    decl_try_err();

    if ((err = Driver_Start(pChild)) == EOK) {
        Driver_AdoptChild(self, pChild);
    }
    return err;
}

// Removes the given driver from the receiver. The given driver has to be a child
// of the receiver.
void Driver_RemoveChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull pChild)
{
    bool isChild = false;

    if (!Driver_IsActive(self)) {
        return;
    }

    List_ForEach(&self->children, struct Driver,
        DriverRef pCurDriver = driver_from_child_qe(pCurNode);

        if (pCurDriver == pChild) {
            isChild = true;
            break;
        }
    );

    if (isChild) {
        List_Remove(&self->children, &pChild->child_qe);
        Object_Release(pChild);
    }
}

// Returns a reference to the child driver with tag 'tag'. NULL is returned if
// no such child driver exists.
DriverRef _Nullable Driver_GetChildWithTag(DriverRef _Nonnull _Locked self, intptr_t tag)
{
    if (!Driver_IsActive(self)) {
        return NULL;
    }

    List_ForEach(&self->children, struct Driver,
        DriverRef pCurDriver = driver_from_child_qe(pCurNode);

        if (pCurDriver->tag == tag) {
            return pCurDriver;
        }
    );

    return NULL;
}


class_func_defs(Driver, Handler,
override_func_def(deinit, Driver, Object)
func_def(onStart, Driver)
func_def(onStop, Driver)
func_def(publish, Driver)
func_def(unpublish, Driver)
override_func_def(open, Driver, Handler)
func_def(onOpen, Driver)
override_func_def(close, Driver, Handler)
);
