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


errno_t DiskContainer_Create(IOChannelRef _Nonnull pChannel, FSContainerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    struct DiskContainer* self = NULL;
    DiskInfo info;
    uint32_t fsprops = 0;

    try(IOChannel_Ioctl(pChannel, kDiskCommand_GetInfo, &info));

    if ((info.properties & kMediaProperty_IsReadOnly) == kMediaProperty_IsReadOnly) {
        fsprops |= kFSProperty_IsReadOnly;
    }
    if ((info.properties & kMediaProperty_IsRemovable) == kMediaProperty_IsRemovable) {
        fsprops |= kFSProperty_IsRemovable;
    }

    try(FSContainer_Create(class(DiskContainer), info.mediaId, info.blockCount, info.blockSize, fsprops, (FSContainerRef*)&self));

    self->diskCache = DiskDriver_GetDiskCache(DriverChannel_GetDriverAs(pChannel, DiskDriver));
    DiskCache_OpenSession(self->diskCache, pChannel, info.mediaId, &self->session);

catch:
    *pOutSelf = (FSContainerRef)self;
    return err;
}

void DiskContainer_deinit(struct DiskContainer* _Nonnull self)
{
    if (self->diskCache) {
        DiskCache_CloseSession(self->diskCache, &self->session);
    }
}

void DiskContainer_disconnect(struct DiskContainer* _Nonnull self)
{
    DiskCache_Sync(self->diskCache, &self->session);
    DiskCache_CloseSession(self->diskCache, &self->session);
}


errno_t DiskContainer_mapBlock(struct DiskContainer* _Nonnull self, LogicalBlockAddress lba, MapBlock mode, FSBlock* _Nonnull blk)
{
    return DiskCache_MapBlock(self->diskCache, &self->session, lba, mode, blk);
}

errno_t DiskContainer_unmapBlock(struct DiskContainer* _Nonnull self, intptr_t token, WriteBlock mode)
{
    return DiskCache_UnmapBlock(self->diskCache, &self->session, token, mode);
}

errno_t DiskContainer_prefetchBlock(struct DiskContainer* _Nonnull self, LogicalBlockAddress lba)
{
    return DiskCache_PrefetchBlock(self->diskCache, &self->session, lba);
}


errno_t DiskContainer_syncBlock(struct DiskContainer* _Nonnull self, LogicalBlockAddress lba)
{
    return DiskCache_SyncBlock(self->diskCache, &self->session, lba);
}

errno_t DiskContainer_sync(struct DiskContainer* _Nonnull self)
{
    return DiskCache_Sync(self->diskCache, &self->session);
}


errno_t DiskContainer_getDiskName(struct DiskContainer* _Nonnull self, size_t bufSize, char* _Nonnull buf)
{
    return DiskCache_GetSessionDiskName(self->diskCache, &self->session, bufSize, buf);
}


class_func_defs(DiskContainer, Object,
override_func_def(deinit, DiskContainer, Object)
override_func_def(disconnect, DiskContainer, FSContainer)
override_func_def(mapBlock, DiskContainer, FSContainer)
override_func_def(unmapBlock, DiskContainer, FSContainer)
override_func_def(prefetchBlock, DiskContainer, FSContainer)
override_func_def(syncBlock, DiskContainer, FSContainer)
override_func_def(sync, DiskContainer, FSContainer)
override_func_def(getDiskName, DiskContainer, FSContainer)
);
