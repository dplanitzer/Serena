//
//  Driver.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Driver.h"
#include "DriverChannel.h"
#include "DriverManager.h"
#include <kpi/fcntl.h>


#define driver_from_child_qe(__ptr) \
(DriverRef) (((uint8_t*)__ptr) - offsetof(struct Driver, child_qe))


errno_t Driver_Create(Class* _Nonnull pClass, unsigned options, CatalogId parentDirectoryId, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverRef self = NULL;

    try(Handler_Create(pClass, options & kHandler_OptionsMask, (HandlerRef*)&self));

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

// Starts the driver. A driver may only be started once. It can not be restarted
// after it has been stopped. A driver is started after the hardware has been
// detected and the driver instance created. The driver should reset the hardware
// and bring it to an idle state. If the driver detects an error while resetting
// the hardware, it should record this error internally and return it from the
// first action method (read, write, etc) that is invoked.
// Note that the actual start code is always executed asynchronously.
errno_t Driver_Start(DriverRef _Nonnull self)
{
    decl_try_err();

    Driver_Lock(self);
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
    Driver_Unlock(self);

    return err;
}

errno_t Driver_onStart(DriverRef _Nonnull _Locked self)
{
    return EOK;
}



void Driver_onStop(DriverRef _Nonnull _Locked self)
{
}

// Terminates a driver. A terminated driver does not access its underlying hardware
// anymore. Terminating a driver also terminates all of its child drivers. Once a
// driver has been terminated, it can not be restarted anymore.
errno_t Driver_Terminate(DriverRef _Nonnull self)
{
    decl_try_err();

    // Change the state to terminating. 
    Driver_Lock(self);
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
    Driver_Unlock(self);

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


    Driver_Lock(self);

    if (err != EOK) {
        self->state = kDriverState_Active;
        Driver_Unlock(self);
        return err;
    }


    // Stop myself
    Driver_OnStop(self);


    // And mark the driver as terminated
    self->state = kDriverState_Terminated;
    Driver_Unlock(self);

    return EOK;
}


// Publish the driver. Should be called from the onStart() override.
errno_t Driver_publish(DriverRef _Nonnull self, const DriverEntry* _Nonnull de)
{
    return DriverManager_Publish(gDriverManager, de, &self->id);
}

// Unpublishes the driver. Should be called from the onStop() override.
void Driver_unpublish(DriverRef _Nonnull self)
{
    DriverManager_Unpublish(gDriverManager, self->id);
    self->id = 0;
}



errno_t Driver_createChannel(DriverRef _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    IOChannelOptions iocOpts = 0;

    if ((self->options & kDriver_Seekable) == kDriver_Seekable) {
        iocOpts |= kIOChannel_Seekable;
    }
    
    return DriverChannel_Create(class(DriverChannel), iocOpts, SEO_FT_DRIVER, mode, self, pOutChannel);
}

errno_t Driver_open(DriverRef _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();

    if (self->openCount > 0 && (self->options & kDriver_Exclusive) == kDriver_Exclusive) {
        *pOutChannel = NULL;
        return EBUSY;
    }

    err = Driver_CreateChannel(self, mode, arg, pOutChannel);    
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

    Driver_Lock(self);
    if (Driver_IsActive(self)) {
        err = Handler_Open(self, mode, arg, pOutChannel);
    }
    else {
        err = ENODEV;
    }
    Driver_Unlock(self);

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

    Driver_Lock(self);
    if (Driver_IsActive(self)) {
        err = Handler_Close(self, pChannel);
    }
    else {
        err = ENODEV;
    }
    Driver_Unlock(self);

    return err;
}

errno_t Driver_SetTag(DriverRef _Nonnull self, intptr_t tag)
{
    Driver_Lock(self);
    if (self->state != kDriverState_Inactive) {
        return EBUSY;
    }

    self->tag = tag;
    Driver_Unlock(self);
    return EOK;
}

// Returns the driver's tag. 0 is returned if the driver has no tag assigned to
// it.
intptr_t Driver_GetTag(DriverRef _Nonnull self)
{
    Driver_Lock(self);
    const intptr_t tag = self->tag;
    Driver_Unlock(self);
    return tag;
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
func_def(createChannel, Driver)
override_func_def(close, Driver, Handler)
);
