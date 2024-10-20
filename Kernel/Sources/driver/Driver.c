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


errno_t _Driver_Create(Class* _Nonnull pClass, DriverModel model, DriverRef _Nullable * _Nonnull pOutDriver)
{
    errno_t err = _Object_Create(pClass, 0, (ObjectRef*)pOutDriver);

    if (err == EOK) {
        err = Driver_Init(*pOutDriver, model);
    }
    else {
        Object_Release(*pOutDriver);
        *pOutDriver = NULL;
    }
    return err;
}

errno_t Driver_Init(DriverRef _Nonnull self, DriverModel model)
{
    decl_try_err();

    self->dispatchQueue = NULL;
    List_Init(&self->children);
    ListNode_Init(&self->childNode);
    self->state = kDriverState_Stopped;

    if (model == kDriverModel_Async) {
        err = DispatchQueue_Create(0, 1, kDispatchQoS_Utility, kDispatchPriority_Normal, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&self->dispatchQueue);
    }

    self->driverId = GetNewDriverId();

    return err;
}

static void Driver_deinit(DriverRef _Nonnull self)
{
    Object_Release(self->dispatchQueue);
    self->dispatchQueue = NULL;
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
    if (self->state != kDriverState_Stopped) {
        return EOK;
    }
    self->state = kDriverState_Running;

    return invoke_0(start, Driver, self);
}

errno_t Driver_start(DriverRef _Nonnull self)
{
    return EOK;
}



// Terminates a driver. A terminated driver does not access its underlying hardware
// anymore. Terminating a driver also terminates all of its child drivers. Once a
// driver has been terminated, it can not be restarted anymore.
void Driver_Terminate(DriverRef _Nonnull self)
{
    if (self->state == kDriverState_Terminated) {
        return;
    }

    
    // First terminate all our child drivers
    List_ForEach(&self->children, struct Driver,
        Driver_Terminate(pCurNode);
    );


    // Stop ourselves
    invoke_0(stop, Driver, self);


    // Stop the dispatch queue if necessary
    if (self->dispatchQueue) {
        DispatchQueue_Terminate(self->dispatchQueue);
        DispatchQueue_WaitForTerminationCompleted(self->dispatchQueue);
    }


    // And mark myself as terminated
    self->state = kDriverState_Terminated;
}

void Driver_stop(DriverRef _Nonnull self)
{
    Driver_Unpublish(self);
}



errno_t Driver_Open(DriverRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    if (self->state != kDriverState_Running) {
        return ENODEV;
    }

    return invoke_n(open, Driver, self, path, mode, pOutChannel);
}

errno_t Driver_open(DriverRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return EIO;
}



errno_t Driver_Close(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel)
{
    return invoke_n(close, Driver, self, pChannel);
}

errno_t Driver_close(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel)
{
    return EOK;
}


errno_t Driver_Read(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    if (self->state == kDriverState_Running) {
        return invoke_n(read, Driver, self, pChannel, pBuffer, nBytesToRead, nOutBytesRead);
    }
    else {
        return ENODEV;
    }
}

errno_t Driver_read(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EBADF;
}



errno_t Driver_Write(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    if (self->state == kDriverState_Running) {
        return invoke_n(write, Driver, self, pChannel, pBuffer, nBytesToWrite, nOutBytesWritten);
    }
    else {
        return ENODEV;
    }
}

errno_t Driver_write(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EBADF;
}



errno_t Driver_Ioctl(DriverRef _Nonnull self, int cmd, va_list ap)
{
    decl_try_err();

    if (self->state == kDriverState_Running) {
        return invoke_n(ioctl, Driver, self, cmd, ap);
    }
    else {
        return ENODEV;
    }
}

errno_t Driver_ioctl(DriverRef _Nonnull self, int cmd, va_list ap)
{
    return ENOTIOCTLCMD;
}



// Publishes the driver instance to the driver catalog with the given name.
errno_t Driver_Publish(DriverRef _Nonnull self, const char* name)
{
    return DriverCatalog_Publish(gDriverCatalog, name, self->driverId, self);
}

// Removes the driver instance from the driver catalog.
void Driver_Unpublish(DriverRef _Nonnull self)
{
    DriverCatalog_Unpublish(gDriverCatalog, self->driverId);
}


// Adds the given driver as a child to the receiver.
void Driver_AddChild(DriverRef _Nonnull self, DriverRef _Nonnull pChild)
{
    Driver_AdoptChild(self, Object_RetainAs(pChild, Driver));
}

// Adds the given driver to the receiver as a child. Consumes the provided strong
// reference.
void Driver_AdoptChild(DriverRef _Nonnull self, DriverRef _Nonnull _Consuming pChild)
{
    List_InsertAfterLast(&self->children, &pChild->childNode);
}

// Removes the given driver from the receiver. The given driver has to be a child
// of the receiver.
void Driver_RemoveChild(DriverRef _Nonnull self, DriverRef _Nonnull pChild)
{
    bool isChild = false;

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
    func_def(start, Driver)
    func_def(stop, Driver)
    func_def(open, Driver)
    func_def(close, Driver)
    func_def(read, Driver)
    func_def(write, Driver)
    func_def(ioctl, Driver)
);
