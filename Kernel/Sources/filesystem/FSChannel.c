//
//  FSChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "FSChannel.h"
#include "Filesystem.h"


errno_t FSChannel_Create(Class* _Nonnull pClass, IOChannelOptions options, int channelType, unsigned int mode, FilesystemRef _Nonnull fs, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FSChannelRef self = NULL;

    if ((err = IOChannel_Create(pClass, options, kIOChannelType_Driver, mode, (IOChannelRef*)&self)) == EOK) {
        self->fs = Object_RetainAs(fs, Filesystem);
    }

    *pOutSelf = (IOChannelRef)self;
    return err;
}

errno_t FSChannel_finalize(FSChannelRef _Nonnull self)
{
    if (self->fs) {
        Filesystem_Close(self->fs, (IOChannelRef)self);
        
        Object_Release(self->fs);
        self->fs = NULL;
    }

    return EOK;
}

errno_t FSChannel_ioctl(FSChannelRef _Nonnull _Locked self, int cmd, va_list ap)
{
    if (IsIOChannelCommand(cmd)) {
        return super_n(ioctl, IOChannel, FSChannel, self, cmd, ap);
    }
    else {
        return Filesystem_vIoctl(self->fs, cmd, ap);
    }
}

errno_t FSChannel_read(FSChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EPERM;
}

errno_t FSChannel_write(FSChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EPERM;
}


class_func_defs(FSChannel, IOChannel,
override_func_def(finalize, FSChannel, IOChannel)
override_func_def(ioctl, FSChannel, IOChannel)
override_func_def(read, FSChannel, IOChannel)
override_func_def(write, FSChannel, IOChannel)
);
