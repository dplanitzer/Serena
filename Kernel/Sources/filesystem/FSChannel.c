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

    if ((err = IOChannel_Create(pClass, options, SEO_FT_DRIVER, mode, (IOChannelRef*)&self)) == EOK) {
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
    return Filesystem_vIoctl(self->fs, (IOChannelRef)self, cmd, ap);
}


class_func_defs(FSChannel, IOChannel,
override_func_def(finalize, FSChannel, IOChannel)
override_func_def(ioctl, FSChannel, IOChannel)
);
