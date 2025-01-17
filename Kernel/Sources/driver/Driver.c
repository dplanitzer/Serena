//
//  Driver.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Driver.h"
#include "DriverChannel.h"
#include "DriverCatalog.h"

#define DriverFromChildNode(__ptr) \
(DriverRef) (((uint8_t*)__ptr) - offsetof(struct Driver, childNode))


errno_t _Driver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverRef self = NULL;

    try(_Object_Create(pClass, 0, (ObjectRef*)&self));

    Lock_Init(&self->lock);
    List_Init(&self->children);
    ListNode_Init(&self->childNode);
    self->options = options;
    self->state = kDriverState_Inactive;

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
        DriverRef pCurDriver = DriverFromChildNode(pCurNode);

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
        err = Driver_Terminate(DriverFromChildNode(pCurNode));
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

    Driver_Unpublish(self);


    // Stop myself
    Driver_OnStop(self);


    // And mark the driver as terminated
    self->state = kDriverState_Terminated;
    Driver_Unlock(self);

    return EOK;
}



errno_t Driver_createChannel(DriverRef _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    IOChannelOptions iocOpts = 0;

    if ((self->options & kDriver_Seekable) == kDriver_Seekable) {
        iocOpts |= kIOChannel_Seekable;
    }
    
    return DriverChannel_Create(&kDriverChannelClass, iocOpts, kIOChannelType_Driver, mode, self, pOutChannel);
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
        err = invoke_n(open, Driver, self, mode, arg, pOutChannel);
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
        err = invoke_n(close, Driver, self, pChannel);
    }
    else {
        err = ENODEV;
    }
    Driver_Unlock(self);

    return err;
}

errno_t Driver_read(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EBADF;
}

errno_t Driver_write(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EBADF;
}

FileOffset Driver_getSeekableRange(DriverRef _Nonnull self)
{
    return 0ll;
}

errno_t Driver_ioctl(DriverRef _Nonnull self, int cmd, va_list ap)
{
    return ENOTIOCTLCMD;
}

errno_t Driver_Ioctl(DriverRef _Nonnull self, int cmd, ...)
{
    decl_try_err();

    va_list ap;
    va_start(ap, cmd);
    err = Driver_vIoctl(self, cmd, ap);
    va_end(ap);

    return err;
}


errno_t Driver_onPublish(DriverRef _Nonnull _Locked self)
{
    return EOK;
}

// Publishes the driver instance to the driver catalog with the given name.
errno_t Driver_Publish(DriverRef _Nonnull _Locked self, const char* name, intptr_t arg)
{
    decl_try_err();

    if ((err = DriverCatalog_Publish(gDriverCatalog, name, self, arg, &self->driverCatalogId)) == EOK) {
        if ((err = Driver_OnPublish(self)) == EOK) {
            return EOK;
        }

        DriverCatalog_Unpublish(gDriverCatalog, self->driverCatalogId);
    }
    return err;
}

void Driver_onUnpublish(DriverRef _Nonnull _Locked self)
{
}

// Removes the driver instance from the driver catalog.
void Driver_Unpublish(DriverRef _Nonnull _Locked self)
{
    Driver_OnUnpublish(self);
    DriverCatalog_Unpublish(gDriverCatalog, self->driverCatalogId);
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
        List_InsertAfterLast(&self->children, &pChild->childNode);
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
        DriverRef pCurDriver = DriverFromChildNode(pCurNode);

        if (pCurDriver == pChild) {
            isChild = true;
            break;
        }
    );

    if (isChild) {
        List_Remove(&self->children, &pChild->childNode);
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
        DriverRef pCurDriver = DriverFromChildNode(pCurNode);

        if (pCurDriver->tag == tag) {
            return pCurDriver;
        }
    );

    return NULL;
}


class_func_defs(Driver, Object,
override_func_def(deinit, Driver, Object)
func_def(onStart, Driver)
func_def(onStop, Driver)
func_def(onPublish, Driver)
func_def(onUnpublish, Driver)
func_def(open, Driver)
func_def(createChannel, Driver)
func_def(close, Driver)
func_def(read, Driver)
func_def(write, Driver)
func_def(getSeekableRange, Driver)
func_def(ioctl, Driver)
);
