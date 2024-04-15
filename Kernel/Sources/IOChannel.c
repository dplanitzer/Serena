//
//  IOChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "IOChannel.h"


// Creates an instance of an I/O channel. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
errno_t IOChannel_AbstractCreate(Class* _Nonnull pClass, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef self;

    try(kalloc_cleared(pClass->instanceSize, (void**) &self));
    self->super.clazz = pClass;
    Lock_Init(&self->countLock);
    self->ownerCount = 1;
    self->useCount = 0;
    self->mode = mode & (kOpen_ReadWrite | kOpen_Append);

catch:
    *pOutChannel = self;
    return err;
}

static errno_t _IOChannel_Finalize(IOChannelRef _Nonnull self)
{
    decl_try_err();

    err = invoke_0(finalize, IOChannel, self);
    Lock_Deinit(&self->countLock);
    kfree(self);

    return err;
}

void IOChannel_Retain(IOChannelRef _Nonnull self)
{
    Lock_Lock(&self->countLock);
    self->ownerCount++;
    Lock_Unlock(&self->countLock);
}

errno_t IOChannel_Release(IOChannelRef _Nullable self)
{
    bool doFinalize = false;

    Lock_Lock(&self->countLock);
    if (self->ownerCount >= 1) {
        self->ownerCount--;
        if (self->ownerCount == 0 && self->useCount == 0) {
            self->ownerCount = -1;  // Acts as a signal that we triggered finalization
            doFinalize = true;
        }
    }
    Lock_Unlock(&self->countLock);

    if (doFinalize) {
        // Can be triggered at most once. Thus no need to hold the lock while
        // running finalization
        return _IOChannel_Finalize(self);
    }
    else {
        return EOK;
    }
}

void IOChannel_BeginOperation(IOChannelRef _Nonnull self)
{
    Lock_Lock(&self->countLock);
    self->useCount++;
    Lock_Unlock(&self->countLock);
}

void IOChannel_EndOperation(IOChannelRef _Nonnull self)
{
    bool doFinalize = false;

    Lock_Lock(&self->countLock);
    if (self->useCount >= 1) {
        self->useCount--;
        if (self->useCount == 0 && self->ownerCount == 0) {
            self->ownerCount = -1;
            doFinalize = true;
        }
    }
    Lock_Unlock(&self->countLock);

    if (doFinalize) {
        // Can be triggered at most once. Thus no need to hold the lock while
        // running finalization
        _IOChannel_Finalize(self);
    }
}

errno_t IOChannel_finalize(IOChannelRef _Nonnull self)
{
    return EOK;
}

errno_t IOChannel_copy(IOChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return EBADF;
}

errno_t IOChannel_ioctl(IOChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetMode:
            *((unsigned int*) va_arg(ap, unsigned int*)) = self->mode;
            return EOK;

        default:
            return ENOTIOCTLCMD;
    }
}

errno_t IOChannel_read(IOChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EBADF;
}

errno_t IOChannel_Read(IOChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    if ((self->mode & kOpen_Read) == kOpen_Read) {
        return invoke_n(read, IOChannel, self, pBuffer, nBytesToRead, nOutBytesRead);
    } else {
        *nOutBytesRead = 0;
        return EBADF;
    }
}

errno_t IOChannel_write(IOChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EBADF;
}

errno_t IOChannel_Write(IOChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    if ((self->mode & kOpen_Write) == kOpen_Write) {
        return invoke_n(write, IOChannel, self, pBuffer, nBytesToWrite, nOutBytesWritten);
    } else {
        *nOutBytesWritten = 0;
        return EBADF;
    }
}

errno_t IOChannel_seek(IOChannelRef _Nonnull self, FileOffset offset, FileOffset* pOutPosition, int whence)
{
    *pOutPosition = 0;
    return ESPIPE;
}


any_subclass_func_defs(IOChannel,
func_def(finalize, IOChannel)
func_def(copy, IOChannel)
func_def(ioctl, IOChannel)
func_def(read, IOChannel)
func_def(write, IOChannel)
func_def(seek, IOChannel)
);
