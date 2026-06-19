//
//  DiskContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskContainer.h"
#include <assert.h>
#include <driver/disk/DiskDriver.h>
#include <filesystem/Filesystem.h>
#include <filesystem/Inode.h>
#include <handler/DriverHandler.h>
#include <kpi/file.h>

final_class_ivars(DiskContainer, FSContainer,
    InodeRef _Nonnull       diskNode;
    DiskDriverRef _Nonnull  driver;
    DiskCacheRef _Nonnull   diskCache;
    DiskSession             session;
);


errno_t DiskContainer_Create(InodeRef _Locked _Nonnull diskNode, unsigned int mode, FSContainerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskDriverRef disk = (DiskDriverRef)Inode_CopyResource(diskNode);
    struct DiskContainer* self = NULL;
    disk_info_t info;
    uint32_t flags = 0;

    assert(instanceof(disk, DiskDriver));
    try(Driver_Open(disk, mode, 0, NULL));
    try(DiskDriver_GetDiskInfo(disk, &info));

    if ((info.flags & DISK_FLAG_READ_ONLY) == DISK_FLAG_READ_ONLY) {
        flags |= FS_FLAG_READ_ONLY;
    }
    if ((info.flags & DISK_FLAG_REMOVABLE) == DISK_FLAG_REMOVABLE) {
        flags |= FS_FLAG_REMOVABLE;
    }

    DiskSession s;
    DiskCache_OpenSession(gDiskCache, disk, &info, &s);

    try(FSContainer_Create(class(DiskContainer), info.sectorsPerDisk / s.s2bFactor, DiskCache_GetBlockSize(gDiskCache), flags, (FSContainerRef*)&self));
    self->diskNode = Inode_Reacquire(diskNode);
    self->driver = disk;
    self->diskCache = gDiskCache;
    self->session = s;

catch:
    if (err != EOK && disk) {
        Driver_Close(disk);
        Object_Release(disk);
    }
    *pOutSelf = (FSContainerRef)self;
    
    return err;
}

void DiskContainer_deinit(DiskContainerRef _Nonnull self)
{
    if (self->diskCache) {
        DiskCache_CloseSession(self->diskCache, &self->session);
        self->diskCache = NULL;
    }

    if (self->driver) {
        Driver_Close(self->driver);
        Object_Release(self->driver);
        self->driver = NULL;
    }

    if (self->diskNode) {
        Inode_Relinquish(self->diskNode);
        self->diskNode = NULL;
    }
}

void DiskContainer_disconnect(DiskContainerRef _Nonnull self)
{
    DiskCache_Sync(self->diskCache, &self->session);
    DiskCache_CloseSession(self->diskCache, &self->session);
}

errno_t DiskContainer_mapBlock(DiskContainerRef _Nonnull self, blkno_t lba, MapBlock mode, FSBlock* _Nonnull blk)
{
    return DiskCache_MapBlock(self->diskCache, &self->session, lba, mode, blk);
}

errno_t DiskContainer_unmapBlock(DiskContainerRef _Nonnull self, intptr_t token, WriteBlock mode)
{
    return DiskCache_UnmapBlock(self->diskCache, &self->session, token, mode);
}

errno_t DiskContainer_prefetchBlock(DiskContainerRef _Nonnull self, blkno_t lba)
{
    return DiskCache_PrefetchBlock(self->diskCache, &self->session, lba);
}


errno_t DiskContainer_syncBlock(DiskContainerRef _Nonnull self, blkno_t lba)
{
    return DiskCache_SyncBlock(self->diskCache, &self->session, lba);
}

errno_t DiskContainer_sync(DiskContainerRef _Nonnull self)
{
    return DiskCache_Sync(self->diskCache, &self->session);
}


errno_t DiskContainer_getDiskInfo(DiskContainerRef _Nonnull self, disk_info_t* _Nonnull info)
{
    return DiskDriver_GetDiskInfo(self->driver, info);
}

InodeRef DiskContainer_getDiskNode(DiskContainerRef _Nonnull self)
{
    return self->diskNode;
}


class_func_defs(DiskContainer, FSContainer,
override_func_def(deinit, DiskContainer, Object)
override_func_def(disconnect, DiskContainer, FSContainer)
override_func_def(mapBlock, DiskContainer, FSContainer)
override_func_def(unmapBlock, DiskContainer, FSContainer)
override_func_def(prefetchBlock, DiskContainer, FSContainer)
override_func_def(syncBlock, DiskContainer, FSContainer)
override_func_def(sync, DiskContainer, FSContainer)
override_func_def(getDiskInfo, DiskContainer, FSContainer)
override_func_def(getDiskNode, DiskContainer, FSContainer)
);
