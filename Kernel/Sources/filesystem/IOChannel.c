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
        mtx_init(&self->countLock);
        self->ownerCount = 1;
        self->useCount = 0;
        self->mode = mode & (O_ACCMODE | O_FILESTATUS);
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

    mtx_deinit(&self->countLock);
    kfree(self);

    return err;
}

IOChannelRef IOChannel_Retain(IOChannelRef _Nonnull self)
{
    mtx_lock(&self->countLock);
    self->ownerCount++;
    mtx_unlock(&self->countLock);

    return self;
}

errno_t IOChannel_Release(IOChannelRef _Nullable self)
{
    if (self) {
        bool doFinalize = false;

        mtx_lock(&self->countLock);
        if (self->ownerCount >= 1) {
            self->ownerCount--;
            if (self->ownerCount == 0 && self->useCount == 0) {
                self->ownerCount = -1;  // Acts as a signal that we triggered finalization
                doFinalize = true;
            }
        }
        mtx_unlock(&self->countLock);

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
    mtx_lock(&self->countLock);
    self->useCount++;
    mtx_unlock(&self->countLock);
}

void IOChannel_EndOperation(IOChannelRef _Nonnull self)
{
    bool doFinalize = false;

    mtx_lock(&self->countLock);
    if (self->useCount >= 1) {
        self->useCount--;
        if (self->useCount == 0 && self->ownerCount == 0) {
            self->ownerCount = -1;
            doFinalize = true;
        }
    }
    mtx_unlock(&self->countLock);

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


errno_t IOChannel_read(IOChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EBADF;
}

errno_t IOChannel_write(IOChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EBADF;
}


errno_t IOChannel_DoSeek(IOChannelRef _Nonnull self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();

    if (whence == SEEK_SET) {
        if (offset >= 0ll) {
            self->offset = offset;
        }
        else {
            throw(EINVAL);
        }
    }
    else if(whence == SEEK_CUR || whence == SEEK_END) {
        const off_t refPos = (whence == SEEK_END) ? IOChannel_GetSeekableRange(self) : self->offset;
        
        if (offset < 0ll && -offset > refPos) {
            throw(EINVAL);
        }
        else {
            const off_t newOffset = refPos + offset;

            if (newOffset < 0ll) {
                throw(EOVERFLOW);
            }

            self->offset = newOffset;
        }
    }
    else {
        throw(EINVAL);
    }

    if (pOutNewPos) {
        *pOutNewPos = self->offset;
    }

catch:
    return err;
}

errno_t IOChannel_seek(IOChannelRef _Nonnull self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    return ESPIPE;
}

off_t IOChannel_getSeekableRange(IOChannelRef _Nonnull _Locked self)
{
    return 0ll;
}


errno_t IOChannel_vFcntl(IOChannelRef _Nonnull self, int cmd, int* _Nonnull pResult, va_list ap)
{
    decl_try_err();

    switch (cmd) {
        case F_GETFD:
            *pResult = 0;
            break;

        case F_GETFL:
            *pResult = self->mode;
            break;

        case F_SETFL: { //XXX should be atomic_int
            const int flags = va_arg(ap, int);

            self->mode = (flags & O_FILESTATUS);
            break;
        }

        case F_UPDTFL: {    //XXX should be atomic_int
            const int setOrClear = va_arg(ap, int);
            const int fl = va_arg(ap, int) & O_FILESTATUS;

            if (setOrClear) {
                self->mode |= fl;
            }
            else {
                self->mode &= ~fl;
            }
            break;
        }

        case F_GETTYPE:
            *pResult = self->channelType;
            break;

        default:
            *pResult = -1;
            err = EINVAL;
            break;
    }

    return err;
}


errno_t IOChannel_ioctl(IOChannelRef _Nonnull self, int cmd, va_list ap)
{
    return ENOTIOCTLCMD;
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


any_subclass_func_defs(IOChannel,
func_def(finalize, IOChannel)
func_def(ioctl, IOChannel)
func_def(read, IOChannel)
func_def(write, IOChannel)
func_def(seek, IOChannel)
func_def(getSeekableRange, IOChannel)
);
