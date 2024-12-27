//
//  PartitionDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PartitionDriver.h"


#define MAX_NAME_LENGTH 8

// All ivars are protected by the dispatch queue
final_class_ivars(PartitionDriver, DiskDriver,
    DiskDriverRef _Nonnull _Weak    diskDriver;         // Driver representing the whole disk
    DiskId                          wholeDiskId;        // Disk Id of the whole disk driver
    MediaId                         wholeMediaId;       // Media Id of the whole disk
    LogicalBlockAddress             startBlock;         // First block of the partition
    LogicalBlockCount               blockCount;         // Partition size in terms of blocks
    size_t                          blockSize;
    bool                            isReadOnly;
    char                            name[MAX_NAME_LENGTH];
);


errno_t PartitionDriver_Create(const char* _Nonnull name, LogicalBlockAddress startBlock, LogicalBlockCount blockCount, bool isReadOnly, DiskDriverRef disk, PartitionDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskInfo info;
    PartitionDriverRef self = NULL;

    try(DiskDriver_GetInfo(disk, &info));
    try(DiskDriver_Create(PartitionDriver, &self));
    self->diskDriver = disk;
    self->wholeDiskId = info.diskId;
    self->wholeMediaId = info.mediaId;
    self->startBlock = startBlock;
    self->blockCount = blockCount;
    self->blockSize = info.blockSize;
    self->isReadOnly = info.isReadOnly ? info.isReadOnly : isReadOnly;
    String_CopyUpTo(self->name, name, MAX_NAME_LENGTH);

catch:
    *pOutSelf = self;
    return err;
}

errno_t PartitionDriver_start(PartitionDriverRef _Nonnull self)
{
    return Driver_Publish(self, self->name, 0);
}

errno_t PartitionDriver_getInfo_async(PartitionDriverRef _Nonnull self, DiskInfo* pOutInfo)
{
    decl_try_err();

    err = DiskDriver_GetInfo(self->diskDriver, pOutInfo);
    if (err == EOK) {
        pOutInfo->diskId = DiskDriver_GetDiskId(self);
        pOutInfo->mediaId = self->wholeMediaId;
        pOutInfo->isReadOnly = self->isReadOnly;
        pOutInfo->reserved[0] = 0;
        pOutInfo->reserved[1] = 0;
        pOutInfo->reserved[2] = 0;
        pOutInfo->blockSize = self->blockSize;
        pOutInfo->blockCount = self->blockCount;
    }

    return err;
}

MediaId PartitionDriver_getCurrentMediaId(PartitionDriverRef _Nonnull self)
{
    return self->wholeMediaId;
}

void PartitionDriver_beginIO_async(PartitionDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock, const DiskAddress* _Nonnull targetAddr)
{
    DiskAddress da;

    da.diskId = self->wholeDiskId;
    da.mediaId = self->wholeMediaId;
    da.lba = targetAddr->lba + self->startBlock;
    return DiskDriver_BeginIO(self->diskDriver, pBlock, &da);
}


class_func_defs(PartitionDriver, DiskDriver,
override_func_def(start, PartitionDriver, Driver)
override_func_def(getInfo_async, PartitionDriver, DiskDriver)
override_func_def(getCurrentMediaId, PartitionDriver, DiskDriver)
override_func_def(beginIO_async, PartitionDriver, DiskDriver)
);
