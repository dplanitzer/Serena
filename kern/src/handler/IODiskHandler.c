//
//  IODiskHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IODiskHandler.h"
#include <driver/disk/DiskDriver.h>


errno_t IODiskHandler_Create(DiskDriverRef _Nonnull drv, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    struct IODiskHandler* self;

    err = IODriverHandler_Create(class(IODiskHandler), FD_TYPE_DRIVER, flags, (DriverRef)drv, (HandlerRef*)&self);
    if (err == EOK) {
        mtx_init(&self->mtx);
    }

    *pOutHandler = (HandlerRef)self;
    return err;
}

errno_t IODiskHandler_read(struct IODiskHandler* _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    const fd_flags_t flags = Handler_GetFlags(self);
    DiskDriverRef drv = IODriverHandler_GetDriver(self);

    if ((flags & O_RDONLY) == 0) {
        *nOutBytesRead = 0;
        return EBADF;
    }


    mtx_lock(&self->mtx);
    err = Driver_Read(drv, flags, &self->offset, pBuffer, nBytesToRead, nOutBytesRead);
    mtx_unlock(&self->mtx);

    return err;
}

errno_t IODiskHandler_write(struct IODiskHandler* _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    const fd_flags_t flags = Handler_GetFlags(self);
    DiskDriverRef drv = IODriverHandler_GetDriver(self);

    if ((flags & O_WRONLY) == 0) {
        *nOutBytesWritten = 0;
        return EBADF;
    }


    mtx_lock(&self->mtx);
    err = Driver_Write(drv, flags, &self->offset, pBuffer, nBytesToWrite, nOutBytesWritten);
    mtx_unlock(&self->mtx);

    return err;
}

errno_t IODiskHandler_seek(struct IODiskHandler* _Nonnull self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();
    DiskDriverRef drv = IODriverHandler_GetDriver(self);
    off_t endPos = 0ll;

    if (!Driver_IsSeekable(drv)) {
        return ESPIPE;
    }


    mtx_lock(&self->mtx);
    if (whence == SEEK_END) {
        endPos = Driver_GetSeekableRange(drv);
    }

    err = do_seek(offset, whence, endPos, &self->offset);

    if (pOutNewPos && err == EOK) {
        *pOutNewPos = self->offset;
    }
    mtx_unlock(&self->mtx);

    return err;
}

errno_t IODiskHandler_control(struct IODiskHandler* _Nonnull self, int cmd, va_list ap)
{
    DiskDriverRef drv = IODriverHandler_GetDriver(self);

    switch (cmd) {
        case kDiskCommand_GetDriveInfo: {
            drive_info_t* info = va_arg(ap, drive_info_t*);
            
            return DiskDriver_GetDriveInfo(drv, info);
        }

        case kDiskCommand_GetDiskInfo: {
            disk_info_t* info = va_arg(ap, disk_info_t*);
        
            return DiskDriver_GetDiskInfo(drv, info);
        }

        case kDiskCommand_FormatDisk: {
            const char fillByte = va_arg(ap, int);

            const errno_t err = DiskDriver_FormatDisk(drv, fillByte);
            return err;
        }

        case kDiskCommand_FormatTrack: {
            decl_try_err();
            const char fillByte = va_arg(ap, int);

            mtx_lock(&self->mtx);
            err = DiskDriver_FormatTrack(drv, &self->offset, fillByte);
            mtx_unlock(&self->mtx);
            return err;
        }

        case kDiskCommand_SenseDisk:
            return DiskDriver_SenseDisk(drv);

        default:
            return Handler_Super_Control(IODiskHandler, cmd, ap);
    }
}


class_func_defs(IODiskHandler, IODriverHandler,
override_func_def(read, IODiskHandler, Handler)
override_func_def(write, IODiskHandler, Handler)
override_func_def(seek, IODiskHandler, Handler)
override_func_def(control, IODiskHandler, Handler)
);
