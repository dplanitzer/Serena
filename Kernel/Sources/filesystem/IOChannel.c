//
//  IOChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "IOChannel.h"
#include <kern/kalloc.h>

typedef errno_t (*IOChannel_Finalize_Impl)(void* _Nonnull self);


// Creates an instance of an I/O channel. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
errno_t IOChannel_Create(Class* _Nonnull pClass, IOChannelOptions options, int channelType, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef self;

    if ((err = kalloc_cleared(pClass->instanceSize, (void**) &self)) == EOK) {
        self->super.clazz = pClass;
        Lock_Init(&self->countLock);
        self->ownerCount = 1;
        self->useCount = 0;
        self->mode = mode & (O_RDWR | O_APPEND | O_NONBLOCK);
        self->options = options;
        self->channelType = channelType;
    }
    *pOutChannel = self;
    
    return err;
}

static errno_t _IOChannel_Finalize(IOChannelRef _Nonnull self)
{
    decl_try_err();
    IOChannel_Finalize_Impl pPrevFinalizeImpl = NULL;
    Class* pCurClass = classof(self);

    for(;;) {
        IOChannel_Finalize_Impl pCurFinalizeImpl = (IOChannel_Finalize_Impl)implementationof(finalize, IOChannel, pCurClass);
        
        if (pCurFinalizeImpl != pPrevFinalizeImpl) {
            const errno_t err0 = pCurFinalizeImpl(self);

            if (err == EOK) { err = err0; }
            pPrevFinalizeImpl = pCurFinalizeImpl;
        }

        if (pCurClass == &kIOChannelClass) {
            break;
        }

        pCurClass = pCurClass->super;
    }

    Lock_Deinit(&self->countLock);
    kfree(self);

    return err;
}

IOChannelRef IOChannel_Retain(IOChannelRef _Nonnull self)
{
    Lock_Lock(&self->countLock);
    self->ownerCount++;
    Lock_Unlock(&self->countLock);

    return self;
}

errno_t IOChannel_Release(IOChannelRef _Nullable self)
{
    if (self) {
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
    }
    return EOK;
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


void IOChannel_lock(IOChannelRef _Nonnull self)
{
}

void IOChannel_unlock(IOChannelRef _Nonnull _Locked self)
{
}


errno_t IOChannel_read(IOChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EBADF;
}

errno_t IOChannel_Read(IOChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    IOChannel_Lock(self);
    if ((self->mode & O_RDONLY) == O_RDONLY) {
        err = invoke_n(read, IOChannel, self, pBuffer, nBytesToRead, nOutBytesRead);
    } else {
        *nOutBytesRead = 0;
        err = EBADF;
    }
    IOChannel_Unlock(self);

    return err;
}

errno_t IOChannel_write(IOChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EBADF;
}


errno_t IOChannel_Write(IOChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();

    IOChannel_Lock(self);
    if ((self->mode & O_WRONLY) == O_WRONLY) {
        err = invoke_n(write, IOChannel, self, pBuffer, nBytesToWrite, nOutBytesWritten);
    } else {
        *nOutBytesWritten = 0;
        err = EBADF;
    }
    IOChannel_Unlock(self);

    return err;
}


errno_t IOChannel_seek(IOChannelRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutOldPosition, int whence)
{
    if (whence == SEEK_SET) {
        if (offset >= 0ll) {
            if (pOutOldPosition) {
                *pOutOldPosition = self->offset;
            }

            self->offset = offset;
            return EOK;
        }
        else {
            return EINVAL;
        }
    }
    else if(whence == SEEK_CUR || whence == SEEK_END) {
        const off_t refPos = (whence == SEEK_END) ? IOChannel_GetSeekableRange(self) : self->offset;
            
        if (offset < 0ll && -offset > refPos) {
            return EINVAL;
        }
        else {
            const off_t newOffset = refPos + offset;

            if (newOffset < 0ll) {
                return EOVERFLOW;
            }

            if (pOutOldPosition) {
                *pOutOldPosition = self->offset;
            }
            self->offset = newOffset;
            return EOK;
        }
    }
    else {
        return EINVAL;
    }
}

errno_t IOChannel_Seek(IOChannelRef _Nonnull self, off_t offset, off_t* pOutOldPosition, int whence)
{
    decl_try_err();

    IOChannel_Lock(self);
    if ((self->options & kIOChannel_Seekable) == kIOChannel_Seekable) {
        err = invoke_n(seek, IOChannel, self, offset, pOutOldPosition, whence);
    } else {
        *pOutOldPosition = 0ll;
        err = ESPIPE;
    }
    IOChannel_Unlock(self);

    return err;
}

off_t IOChannel_getSeekableRange(IOChannelRef _Nonnull _Locked self)
{
    return 0ll;
}


errno_t IOChannel_Ioctl(IOChannelRef _Nonnull self, int cmd, ...)
{
    decl_try_err();

    va_list ap;
    va_start(ap, cmd);
    err = IOChannel_vIoctl(self, cmd, ap);
    va_end(ap);

    return err;
}

errno_t IOChannel_vIoctl(IOChannelRef _Nonnull self, int cmd, va_list ap)
{
    decl_try_err();

    IOChannel_Lock(self);
    err = invoke_n(ioctl, IOChannel, self, cmd, ap);
    IOChannel_Unlock(self);

    return err;
}

errno_t IOChannel_ioctl(IOChannelRef _Nonnull _Locked self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetType:
            *((int*) va_arg(ap, int*)) = self->channelType;
            return EOK;

        case kIOChannelCommand_GetMode:
            *((unsigned int*) va_arg(ap, unsigned int*)) = self->mode;
            return EOK;

        case kIOChannelCommand_SetMode: {
            int setOrClear = va_arg(ap, int);
            unsigned int newMode = va_arg(ap, unsigned int) & (O_APPEND | O_NONBLOCK);

            if (setOrClear) {
                self->mode |= newMode;
            }
            else {
                self->mode &= ~newMode;
            }
            return EOK;
        }

        default:
            return ENOTIOCTLCMD;
    }
}


any_subclass_func_defs(IOChannel,
func_def(finalize, IOChannel)
func_def(lock, IOChannel)
func_def(unlock, IOChannel)
func_def(ioctl, IOChannel)
func_def(read, IOChannel)
func_def(write, IOChannel)
func_def(seek, IOChannel)
func_def(getSeekableRange, IOChannel)
);
