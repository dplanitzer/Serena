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


////////////////////////////////////////////////////////////////////////////////
// MARK: Driver
// MARK: -
////////////////////////////////////////////////////////////////////////////////

typedef struct IOReadRequest {
    IOChannelRef _Nonnull   channel;
    char* _Nonnull          inBuffer;
    ssize_t                 inByteCount;
    ssize_t                 outByteCount;
    errno_t                 outStatus;
} IOReadRequest;

typedef struct IOWriteRequest {
    IOChannelRef _Nonnull   channel;
    const char* _Nonnull    outBuffer;
    ssize_t                 inByteCount;
    ssize_t                 outByteCount;
    errno_t                 outStatus;
} IOWriteRequest;

typedef struct IOControlRequest {
    int         inCmd;
    va_list     inAp;
    errno_t     outStatus;
} IOControlRequest;


errno_t _Driver_Create(Class* _Nonnull pClass, DriverModel model, unsigned int options, DriverRef _Nullable * _Nonnull pOutDriver)
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

errno_t Driver_Init(DriverRef _Nonnull self, DriverModel model, unsigned int options)
{
    decl_try_err();

    self->dispatchQueue = NULL;
    self->options = (uint16_t)options;
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



static void Driver_onStart(DriverRef _Nonnull self)
{
    invoke_0(start, Driver, self);
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

    if (!AtomicBool_Set(&self->isStopped, false)) {
        return EOK;
    }

    if (self->dispatchQueue) {
        err = DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)Driver_onStart, self, NULL, 0, 0, 0);
    }
    else {
        invoke_0(start, Driver, self);
    }

    return err;
}

errno_t Driver_start(DriverRef _Nonnull self)
{
    return EOK;
}



static void Driver_onStop(DriverRef _Nonnull self)
{
    invoke_0(stop, Driver, self);
}

// Stops an active driver. Once stopped, a driver does not access its hardware
// anymore. The actual stop function is always executed asynchronously. However
// the caller of Driver_Stop() is blocked until the driver has truly stopped if
// 'waitForCompletion' is true.
void Driver_Stop(DriverRef _Nonnull self, bool waitForCompletion)
{
    if (AtomicBool_Set(&self->isStopped, true)) {
        return;
    }

    if (self->dispatchQueue) {
        DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)Driver_onStop, self, NULL, 0, (waitForCompletion) ? kDispatchOption_Sync : 0, 0);
    }
    else {
        invoke_0(stop, Driver, self);
    }
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


static void Driver_onRead(DriverRef _Nonnull self, IOReadRequest* _Nonnull request)
{
    if (!self->isStopped) {
        request->outStatus = invoke_n(read, Driver, self, request->channel, request->inBuffer, request->inByteCount, &request->outByteCount);
    }
    else {
        request->outByteCount = 0;
        request->outStatus = ENODEV;
    }
}

errno_t Driver_Read(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    IOReadRequest r;

    if (self->isStopped) {
        err = ENODEV;
    }
    else if (self->dispatchQueue) {
        r.channel = pChannel;
        r.inBuffer = pBuffer;
        r.inByteCount = nBytesToRead;

        err = DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)Driver_onRead, self, &r, 0, kDispatchOption_Sync, 0);
        if (err == EOK) {
            *nOutBytesRead = r.outByteCount;
            err = r.outStatus;
        }
        else {
            *nOutBytesRead = 0;
        }
    }
    else {
        err = invoke_n(read, Driver, self, pChannel, pBuffer, nBytesToRead, nOutBytesRead);
    }

    return err;
}

errno_t Driver_read(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EBADF;
}



static void Driver_onWrite(DriverRef _Nonnull self, IOWriteRequest* _Nonnull request)
{
    if (!self->isStopped) {
        request->outStatus = invoke_n(write, Driver, self, request->channel, request->outBuffer, request->inByteCount, &request->outByteCount);
    }
    else {
        request->outByteCount = 0;
        request->outStatus = ENODEV;
    }
}

errno_t Driver_Write(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    IOWriteRequest r;

    if (self->isStopped) {
        err = ENODEV;
    }
    else if (self->dispatchQueue) {
        r.channel = pChannel;
        r.outBuffer = pBuffer;
        r.inByteCount = nBytesToWrite;

        err = DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)Driver_onWrite, self, &r, 0, kDispatchOption_Sync, 0);
        if (err == EOK) {
            *nOutBytesWritten = r.outByteCount;
            err = r.outStatus;
        }
        else {
            *nOutBytesWritten = 0;
        }
    }
    else {
        err = invoke_n(write, Driver, self, pChannel, pBuffer, nBytesToWrite, nOutBytesWritten);
    }

    return err;
}

errno_t Driver_write(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EBADF;
}



static void Driver_onIoctl(DriverRef _Nonnull self, IOControlRequest* _Nonnull request)
{
    if (!self->isStopped) {
        request->outStatus = invoke_n(ioctl, Driver, self, request->inCmd, request->inAp);
    }
    else {
        request->outStatus = ENODEV;
    }
}

errno_t Driver_Ioctl(DriverRef _Nonnull self, int cmd, va_list ap)
{
    decl_try_err();
    IOControlRequest r;

    if (self->isStopped) {
        err = ENODEV;
    }
    else if (self->dispatchQueue) {
        r.inCmd = cmd;
        r.inAp = ap;

        err = DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)Driver_onIoctl, self, &r, 0, kDispatchOption_Sync, 0);
        if (err == EOK) {
            err = r.outStatus;
        }
    }
    else {
        err = invoke_n(ioctl, Driver, self, cmd, ap);
    }

    return err;
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


////////////////////////////////////////////////////////////////////////////////
// MARK: DriverController
// MARK: -
////////////////////////////////////////////////////////////////////////////////

class_func_defs(DriverController, Driver,
);
