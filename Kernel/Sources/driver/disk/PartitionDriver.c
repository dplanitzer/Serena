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
    try(DiskDriver_Create(PartitionDriver, 0, &self));
    self->diskDriver = disk;
    self->wholeDiskId = info.diskId;
    self->wholeMediaId = info.mediaId;
    self->startBlock = startBlock;
    self->blockCount = blockCount;
    self->blockSize = info.blockSize;
    self->isReadOnly = info.isReadOnly ? info.isReadOnly : isReadOnly;
    String_CopyUpTo(self->name, name, MAX_NAME_LENGTH);

    MediaInfo mediaInfo;
    mediaInfo.blockCount = self->blockCount;
    mediaInfo.blockSize = self->blockSize;
    mediaInfo.isReadOnly = self->isReadOnly;
    DiskDriver_NoteMediaLoaded((DiskDriverRef)self, &mediaInfo);

catch:
    *pOutSelf = self;
    return err;
}

errno_t PartitionDriver_onStart(PartitionDriverRef _Nonnull _Locked self)
{
    return Driver_Publish((DriverRef)self, self->name, 0);
}

void PartitionDriver_beginIO(PartitionDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    pBlock->physicalAddress.diskId = self->wholeDiskId;
    pBlock->physicalAddress.lba += self->startBlock;

    DiskDriver_BeginIO(self->diskDriver, pBlock);
}


class_func_defs(PartitionDriver, DiskDriver,
override_func_def(onStart, PartitionDriver, Driver)
override_func_def(beginIO, PartitionDriver, DiskDriver)
);
