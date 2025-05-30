//
//  DiskContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskContainer.h"
#include <driver/DriverChannel.h>
#include <driver/disk/DiskDriver.h>
#include <filesystem/IOChannel.h>
#include <kpi/fcntl.h>


errno_t DiskContainer_Create(IOChannelRef _Nonnull pChannel, FSContainerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    struct DiskContainer* self = NULL;
    disk_info_t info;
    uint32_t fsprops = 0;

    try(IOChannel_Ioctl(pChannel, kDiskCommand_GetDiskInfo, &info));

    if ((info.properties & kDisk_IsReadOnly) == kDisk_IsReadOnly) {
        fsprops |= kFSProperty_IsReadOnly;
    }
    if ((info.properties & kDisk_IsRemovable) == kDisk_IsRemovable) {
        fsprops |= kFSProperty_IsRemovable;
    }

    //XXX select the disk cache based on FS needs and driver sector size
    DiskCacheRef diskCache = gDiskCache;
    DiskSession s;
    DiskCache_OpenSession(diskCache, pChannel, &info, &s);

    try(FSContainer_Create(class(DiskContainer), info.sectorsPerDisk / s.s2bFactor, DiskCache_GetBlockSize(diskCache), fsprops, (FSContainerRef*)&self));
    self->diskCache = diskCache;
    self->session = s;

catch:
    *pOutSelf = (FSContainerRef)self;
    return err;
}

void DiskContainer_deinit(struct DiskContainer* _Nonnull self)
{
    if (self->diskCache) {
        DiskCache_CloseSession(self->diskCache, &self->session);
        self->diskCache = NULL;
    }
}

void DiskContainer_disconnect(struct DiskContainer* _Nonnull self)
{
    DiskCache_Sync(self->diskCache, &self->session);
    DiskCache_CloseSession(self->diskCache, &self->session);
}


errno_t DiskContainer_mapBlock(struct DiskContainer* _Nonnull self, blkno_t lba, MapBlock mode, FSBlock* _Nonnull blk)
{
    return DiskCache_MapBlock(self->diskCache, &self->session, lba, mode, blk);
}

errno_t DiskContainer_unmapBlock(struct DiskContainer* _Nonnull self, intptr_t token, WriteBlock mode)
{
    return DiskCache_UnmapBlock(self->diskCache, &self->session, token, mode);
}

errno_t DiskContainer_prefetchBlock(struct DiskContainer* _Nonnull self, blkno_t lba)
{
    return DiskCache_PrefetchBlock(self->diskCache, &self->session, lba);
}


errno_t DiskContainer_syncBlock(struct DiskContainer* _Nonnull self, blkno_t lba)
{
    return DiskCache_SyncBlock(self->diskCache, &self->session, lba);
}

errno_t DiskContainer_sync(struct DiskContainer* _Nonnull self)
{
    return DiskCache_Sync(self->diskCache, &self->session);
}

errno_t DiskContainer_getDiskInfo(struct DiskContainer* _Nonnull self, disk_info_t* _Nonnull info)
{
    return IOChannel_Ioctl(self->session.channel, kDiskCommand_GetDiskInfo, info);
}


class_func_defs(DiskContainer, Object,
override_func_def(deinit, DiskContainer, Object)
override_func_def(disconnect, DiskContainer, FSContainer)
override_func_def(mapBlock, DiskContainer, FSContainer)
override_func_def(unmapBlock, DiskContainer, FSContainer)
override_func_def(prefetchBlock, DiskContainer, FSContainer)
override_func_def(syncBlock, DiskContainer, FSContainer)
override_func_def(sync, DiskContainer, FSContainer)
override_func_def(getDiskInfo, DiskContainer, FSContainer)
);
