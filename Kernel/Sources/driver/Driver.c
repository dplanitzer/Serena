//
//  Driver.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Driver.h"
#include "DriverChannel.h"
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
    self->isStopped = true;

    if (model == kDriverModel_Async) {
        err = DispatchQueue_Create(0, 1, kDispatchQoS_Utility, kDispatchPriority_Normal, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&self->dispatchQueue);
    }

    return err;
}

static void Driver_deinit(DriverRef _Nonnull self)
{
    if (self->dispatchQueue) {
        DispatchQueue_Terminate(self->dispatchQueue);
        DispatchQueue_WaitForTerminationCompleted(self->dispatchQueue);
        Object_Release(self->dispatchQueue);
        self->dispatchQueue = NULL;
    }
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
    if (!AtomicBool_Set(&self->isStopped, false)) {
        return EOK;
    }

    return invoke_0(start, Driver, self);
}

errno_t Driver_start(DriverRef _Nonnull self)
{
    return EOK;
}



// Stops an active driver. Once stopped, a driver does not access its hardware
// anymore.
void Driver_Stop(DriverRef _Nonnull self)
{
    if (AtomicBool_Set(&self->isStopped, true)) {
        return;
    }

    invoke_0(stop, Driver, self);
}

void Driver_stop(DriverRef _Nonnull self)
{
}



errno_t Driver_Open(DriverRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    if (self->isStopped) {
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
    if (!self->isStopped) {
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
    if (!self->isStopped) {
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

    if (!self->isStopped) {
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
