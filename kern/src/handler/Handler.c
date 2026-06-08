//
//  Handler.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Handler.h"


errno_t Handler_Create(Class* _Nonnull pClass, int type, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    HandlerRef self;

    err = Object_Create(pClass, 0, (void**)&self);
    if (err == EOK) {
        self->mode = mode & (O_ACCMODE | O_FLAGS);
        self->type = type;

        atomic_init(&self->descriptorCount, 0);
    }
    *pOutHandler = self;
    
    return err;
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

errno_t Handler_shutdown(HandlerRef _Nonnull self)
{
    return EOK;
}


class_func_defs(Handler, Object,
func_def(read, Handler)
func_def(write, Handler)
func_def(seek, Handler)
func_def(getSeekableRange, Handler)
func_def(getAttributes, Handler)
func_def(truncate, Handler)
func_def(shutdown, Handler)
);
