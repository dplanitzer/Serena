//
//  DiskDriverChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriverChannel.h"


errno_t DiskDriverChannel_Create(DiskDriverRef _Nonnull pDriver, const DiskInfo* _Nonnull info, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskDriverChannelRef self;

    err = DriverChannel_Create(&kDiskDriverChannelClass, kIOChannel_Seekable, kIOChannelType_Driver, mode, (DriverRef)pDriver, (IOChannelRef*)&self);
    if (err == EOK) {
        self->diskSize = (FileOffset)info->blockCount * (FileOffset)info->blockSize;
        self->info = *info;
    }

    *pOutSelf = (IOChannelRef)self;
    return err;
}

FileOffset DiskDriverChannel_getSeekableRange(DiskDriverChannelRef _Nonnull self)
{
    return self->diskSize;
}


class_func_defs(DiskDriverChannel, DriverChannel,
override_func_def(getSeekableRange, DiskDriverChannel, IOChannel)
);
