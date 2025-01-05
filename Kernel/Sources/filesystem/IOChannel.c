//
//  IOChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "IOChannel.h"

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
        self->mode = mode & (kOpen_ReadWrite | kOpen_Append);
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

errno_t IOChannel_copy(IOChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return EBADF;
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

errno_t IOChannel_ioctl(IOChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetType:
            *((int*) va_arg(ap, int*)) = self->channelType;
            return EOK;

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

errno_t IOChannel_seek(IOChannelRef _Nonnull self, FileOffset offset, FileOffset* pOutOldPosition, int whence)
{
    decl_try_err();
    FileOffset newOffset;

    if ((self->options & kIOChannel_Seekable) == 0) {
        *pOutOldPosition = 0ll;
        return ESPIPE;
    }

    
    switch (whence) {
        case kSeek_Set:
            if (offset < 0ll) {
                err = EINVAL;
            }
            else {
                newOffset = offset;
            }
            break;

        case kSeek_Current:
            if (offset < 0ll && -offset > self->offset) {
                err = EINVAL;
            }
            else {
                newOffset = self->offset + offset;
            }
            break;

        case kSeek_End: {
            const FileOffset fileSize = IOChannel_GetSeekableRange(self);
            if (offset < 0ll && -offset > fileSize) {
                err = EINVAL;
            }
            else {
                newOffset = fileSize + offset;
            }
            break;
        }

        default:
            err = EINVAL;
            break;
    }

    if (err == EOK) {
        if (newOffset < 0 || newOffset > kFileOffset_Max) {
            err = EOVERFLOW;
        }
        else {
            if (pOutOldPosition) {
                *pOutOldPosition = self->offset;
            }
            self->offset = newOffset;
        }
    }

    return err;
}

FileOffset IOChannel_getSeekableRange(IOChannelRef _Nonnull self)
{
    return 0ll;
}


any_subclass_func_defs(IOChannel,
func_def(finalize, IOChannel)
func_def(copy, IOChannel)
func_def(ioctl, IOChannel)
func_def(read, IOChannel)
func_def(write, IOChannel)
func_def(seek, IOChannel)
func_def(getSeekableRange, IOChannel)
);
