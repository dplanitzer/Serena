//
//  Handler.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Handler.h"
#include <kpi/_seek.h>


errno_t Handler_Create(Class* _Nonnull pClass, int type, fd_flags_t oflags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    HandlerRef self;

    err = Object_Create(pClass, 0, (void**)&self);
    if (err == EOK) {
        atomic_int_store(&self->flags, oflags);
        self->type = type;
    }
    *pOutHandler = self;
    
    return err;
}



errno_t Handler_SetFlags(HandlerRef _Nonnull self, int op, int flags)
{
    if ((flags & ~O_FLAGS) != 0) {
        return EINVAL;
    }

    switch (op) {
        case FD_FOP_ADD:
            atomic_int_fetch_or(&self->flags, flags);
            break;

        case FD_FOP_REMOVE:
            atomic_int_fetch_and(&self->flags, ~flags);
            break;

        case FD_FOP_REPLACE:
            atomic_int_store(&self->flags, flags);
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


errno_t do_seek(off_t offset, int whence, off_t endPos, off_t* _Nonnull pos)
{
    if (whence == SEEK_SET) {
        if (offset >= 0ll) {
            *pos = offset;
        }
        else {
            return EINVAL;
        }
    }
    else if(whence == SEEK_CUR || whence == SEEK_END) {
        const off_t refPos = (whence == SEEK_END) ? endPos : *pos;
        
        if (offset < 0ll && -offset > refPos) {
            return EINVAL;
        }
        else {
            const off_t newOffset = refPos + offset;

            if (newOffset < 0ll) {
                return EOVERFLOW;
            }

            *pos = newOffset;
        }
    }
    else {
        return EINVAL;
    }

    return EOK;
}

errno_t Handler_seek(HandlerRef _Nonnull self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    return ESPIPE;
}


errno_t Handler_GetInfo(HandlerRef _Nonnull self, int flavor, fd_info_ref _Nonnull info)
{
    switch (flavor) {
        case FD_INFO_BASIC: {
            fd_basic_info_t* ip = info;
            const fd_flags_t flags = Handler_GetFlags(self);

            ip->type = self->type;
            ip->flags = flags & O_FLAGS;
            ip->access_mode = flags & O_ACCMODE;
            return EOK;
        }
            
        default:
            return EINVAL;
    }
}


class_func_defs(Handler, Object,
func_def(read, Handler)
func_def(write, Handler)
func_def(seek, Handler)
);
