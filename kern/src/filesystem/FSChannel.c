//
//  FSChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "FSChannel.h"
#include "Filesystem.h"

#define _get_fs() \
IOChannel_GetResourceAs(self, Filesystem)


errno_t FSChannel_Create(Class* _Nonnull pClass, int channelType, unsigned int mode, FilesystemRef _Nonnull fs, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FSChannelRef self = NULL;

    if ((err = IOChannel_Create(pClass, SEO_FT_DRIVER, mode, (intptr_t)fs, (IOChannelRef*)&self)) == EOK) {
        Object_Retain(fs);
    }

    *pOutSelf = (IOChannelRef)self;
    return err;
}

errno_t FSChannel_finalize(FSChannelRef _Nonnull self)
{
    FilesystemRef fs = _get_fs();

    if (fs) {
        Filesystem_Close(fs, (IOChannelRef)self);
        Object_Release(fs);
    }

    return EOK;
}

errno_t FSChannel_ioctl(FSChannelRef _Nonnull _Locked self, int cmd, va_list ap)
{
    return Filesystem_vIoctl(_get_fs(), (IOChannelRef)self, cmd, ap);
}


class_func_defs(FSChannel, IOChannel,
override_func_def(finalize, FSChannel, IOChannel)
override_func_def(ioctl, FSChannel, IOChannel)
);
