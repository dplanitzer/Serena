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
#include <dispatchqueue/DispatchQueue.h>

enum {
    kDriverFlag_IsOpen = 2
};


errno_t _Driver_Create(Class* _Nonnull pClass, DriverModel model, DriverOptions options, DriverRef _Nullable * _Nonnull pOutDriver)
{
    errno_t err = _Object_Create(pClass, 0, (ObjectRef*)pOutDriver);

    if (err == EOK) {
        err = Driver_Init(*pOutDriver, model, options);
    }
    else {
        Object_Release(*pOutDriver);
        *pOutDriver = NULL;
    }
    return err;
}

errno_t Driver_Init(DriverRef _Nonnull self, DriverModel model, DriverOptions options)
{
    decl_try_err();

    self->dispatchQueue = NULL;
    Lock_Init(&self->lock);
    List_Init(&self->children);
    ListNode_Init(&self->childNode);
    self->state = kDriverState_Inactive;
    self->model = model;
    self->options = options;
    self->flags = 0;

    if (model == kDriverModel_Async) {
        err = invoke_n(createDispatchQueue, Driver, self, &self->dispatchQueue);
    }

    return err;
}

static void Driver_deinit(DriverRef _Nonnull self)
{
    if (self->dispatchQueue) {
        Object_Release(self->dispatchQueue);
        self->dispatchQueue = NULL;
    }
}


errno_t Driver_createDispatchQueue(DriverRef _Nonnull self, DispatchQueueRef _Nullable * _Nonnull pOutQueue)
{
    return DispatchQueue_Create(0, 1, kDispatchQoS_Utility, kDispatchPriority_Normal, gVirtualProcessorPool, NULL, pOutQueue);
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

    Lock_Lock(&self->lock);
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
    Lock_Unlock(&self->lock);

    return err;
}

errno_t Driver_onStart(DriverRef _Nonnull _Locked self)
{
    return EOK;
}



void Driver_onStop(DriverRef _Nonnull self)
{
}

// Terminates a driver. A terminated driver does not access its underlying hardware
// anymore. Terminating a driver also terminates all of its child drivers. Once a
// driver has been terminated, it can not be restarted anymore.
void Driver_Terminate(DriverRef _Nonnull self)
{
    bool doTerminate = true;

    // Change the state to terminating. 
    Lock_Lock(&self->lock);
    switch (self->state) {
        case kDriverState_Terminating:
        case kDriverState_Terminated:
            doTerminate = false;
            break;

        default:
            self->state = kDriverState_Terminating;
            break;
    }
    Lock_Unlock(&self->lock);

    if (!doTerminate) {
        return;
    }

    
    // The list of child drivers is now frozen and can not change anymore.
    // Synchronously terminate all our child drivers
    List_ForEach(&self->children, struct Driver,
        Driver_Terminate(pCurNode);
    );


    Lock_Lock(&self->lock);
    Driver_Unpublish(self);


    // Stop myself
    Driver_OnStop(self);


    // Stop the dispatch queue if necessary
    if (self->dispatchQueue) {
        DispatchQueue_Terminate(self->dispatchQueue);
        DispatchQueue_WaitForTerminationCompleted(self->dispatchQueue);
    }


    // And mark the driver as terminated
    self->state = kDriverState_Terminated;
    Lock_Unlock(&self->lock);
}



errno_t Driver_open(DriverRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    if (self->state == kDriverState_Active) {
        if ((self->options & kDriver_Exclusive) == kDriver_Exclusive) {
            if ((self->flags & kDriverFlag_IsOpen) == 0) {
                err = invoke_n(createChannel, Driver, self, mode, arg, pOutChannel);
        
                if (err == EOK) {
                    self->flags |= kDriverFlag_IsOpen;
                }
                else {
                    *pOutChannel = NULL;
                }
            }
            else {
                err = EBUSY;
            }
        }
        else {
            err = invoke_n(createChannel, Driver, self, mode, arg, pOutChannel);
        }
    }
    else {
        err = ENODEV;
    }
    Lock_Unlock(&self->lock);

    return err;
}

errno_t Driver_createChannel(DriverRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    DriverChannelOptions dcOpts = 0;

    if ((self->options & kDriver_Seekable) == kDriver_Seekable) {
        dcOpts |= kDriverChannel_Seekable;
    }
    
    return DriverChannel_Create(&kDriverChannelClass, kIOChannelType_Driver, mode, dcOpts, self, pOutChannel);
}

errno_t Driver_close(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel)
{
    if ((self->options & kDriver_Exclusive) == kDriver_Exclusive) {
        Lock_Lock(&self->lock);
        self->flags &= ~kDriverFlag_IsOpen;
        Lock_Unlock(&self->lock);
    }

    return EOK;
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

errno_t Driver_Ioctl(DriverRef _Nonnull self, int cmd, ...)
{
    decl_try_err();

    va_list ap;
    va_start(ap, cmd);
    err = Driver_vIoctl(self, cmd, ap);
    va_end(ap);

    return err;
}

errno_t Driver_ioctl(DriverRef _Nonnull self, int cmd, va_list ap)
{
    return ENOTIOCTLCMD;
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

// Removes the given driver from the receiver. The given driver has to be a child
// of the receiver.
void Driver_RemoveChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull pChild)
{
    bool isChild = false;

    if (!Driver_IsActive(self)) {
        return;
    }

    List_ForEach(&self->children, struct Driver,
        if (pCurNode == pChild) {
            isChild = true;
            break;
        }
    );

    if (isChild) {
        List_Remove(&self->children, &pChild->childNode);
        Object_Release(pChild);
    }
}


class_func_defs(Driver, Object,
override_func_def(deinit, Driver, Object)
func_def(createDispatchQueue, Driver)
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
