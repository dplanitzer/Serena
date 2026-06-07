//
//  Handler.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Handler.h"
#include <kern/kalloc.h>


typedef errno_t (*finalize_func_t)(void* _Nonnull self);


errno_t Handler_Create(Class* _Nonnull pClass, int type, unsigned int mode, intptr_t resource, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    HandlerRef self;

    if ((err = kalloc_cleared(pClass->instanceSize, (void**) &self)) == EOK) {
        self->super.clazz = pClass;
        mtx_init(&self->countLock);
        self->resource = resource;
        self->ownerCount = 1;
        self->useCount = 0;
        self->mode = mode & (O_ACCMODE | O_FLAGS);
        self->type = type;
    }
    *pOutHandler = self;
    
    return err;
}

static errno_t _Handler_Finalize(HandlerRef _Nonnull self)
{
    decl_try_err();
    finalize_func_t pPrevFinalizeImpl = NULL;
    Class* pCurClass = classof(self);

    for(;;) {
        finalize_func_t pCurFinalizeImpl = (finalize_func_t)implementationof(finalize, Handler, pCurClass);
        
        if (pCurFinalizeImpl != pPrevFinalizeImpl) {
            const errno_t err0 = pCurFinalizeImpl(self);

            if (err == EOK) { err = err0; }
            pPrevFinalizeImpl = pCurFinalizeImpl;
        }

        if (pCurClass == &kHandlerClass) {
            break;
        }

        pCurClass = pCurClass->super;
    }

    mtx_deinit(&self->countLock);
    kfree(self);

    return err;
}

HandlerRef Handler_Retain(HandlerRef _Nonnull self)
{
    mtx_lock(&self->countLock);
    self->ownerCount++;
    mtx_unlock(&self->countLock);

    return self;
}

errno_t Handler_Release(HandlerRef _Nullable self)
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
            return _Handler_Finalize(self);
        }
    }
    return EOK;
}

void Handler_BeginOperation(HandlerRef _Nonnull self)
{
    mtx_lock(&self->countLock);
    self->useCount++;
    mtx_unlock(&self->countLock);
}

void Handler_EndOperation(HandlerRef _Nonnull self)
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
        _Handler_Finalize(self);
    }
}



errno_t Handler_finalize(HandlerRef _Nonnull self)
{
    return EOK;
}

int Handler_GetFlags(HandlerRef _Nonnull self)
{
    return self->mode & O_FLAGS; //XXX use atomic_get_int() here
}

errno_t Handler_SetFlags(HandlerRef _Nonnull self, int op, int flags)
{
    if ((flags & ~O_FLAGS) != 0) {
        return EINVAL;
    }

    switch (op) {
        case FD_FOP_ADD:
            self->mode |= flags;    //XXX should be atomic_int()
            break;

        case FD_FOP_REMOVE:
            self->mode &= ~flags;   //XXX should be atomic_int()
            break;

        case FD_FOP_REPLACE:
            self->mode = flags;     //XXX should be atomic_int()
            break;

        default:
            return EINVAL;
    }

    return EOK;
}


errno_t Handler_read(HandlerRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EBADF;
}

errno_t Handler_write(HandlerRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EBADF;
}


errno_t Handler_DoSeek(HandlerRef _Nonnull self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
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
        const off_t refPos = (whence == SEEK_END) ? Handler_GetSeekableRange(self) : self->offset;
        
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

errno_t Handler_seek(HandlerRef _Nonnull self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    return ESPIPE;
}

off_t Handler_getSeekableRange(HandlerRef _Nonnull _Locked self)
{
    return 0ll;
}


errno_t Handler_ioctl(HandlerRef _Nonnull self, int cmd, va_list ap)
{
    return ENOTIOCTLCMD;
}

errno_t Handler_Ioctl(HandlerRef _Nonnull self, int cmd, ...)
{
    decl_try_err();

    va_list ap;
    va_start(ap, cmd);
    err = Handler_vIoctl(self, cmd, ap);
    va_end(ap);

    return err;
}

errno_t Handler_GetInfo(HandlerRef _Nonnull self, int flavor, fd_info_ref _Nonnull info)
{
    switch (flavor) {
        case FD_INFO_BASIC: {
            fd_basic_info_t* ip = info;

            ip->type = self->type;
            ip->flags = self->mode & O_FLAGS;
            ip->access_mode = self->mode & O_ACCMODE;
            return EOK;
        }
            
        default:
            return EINVAL;
    }
}


errno_t Handler_getAttributes(HandlerRef _Nonnull self, fs_attr_t* _Nonnull attr)
{
    return EBADF;
}

errno_t Handler_truncate(HandlerRef _Nonnull self, off_t length)
{
    return EBADF;
}


any_subclass_func_defs(Handler,
func_def(finalize, Handler)
func_def(ioctl, Handler)
func_def(read, Handler)
func_def(write, Handler)
func_def(seek, Handler)
func_def(getSeekableRange, Handler)
func_def(getAttributes, Handler)
func_def(truncate, Handler)
);
