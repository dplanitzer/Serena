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
    DiskDriverRef _Nonnull _Weak    wholeDisk;          // Driver representing the whole disk
    MediaId                         wholeMediaId;       // Media Id of the whole disk
    LogicalBlockAddress             startBlock;         // First block of the partition
    LogicalBlockCount               blockCount;         // Partition size in terms of blocks
    size_t                          blockSize;
    bool                            isReadOnly;
    char                            name[MAX_NAME_LENGTH];
);


errno_t PartitionDriver_Create(DriverRef _Nullable parent, const char* _Nonnull name, LogicalBlockAddress startBlock, LogicalBlockCount blockCount, bool isReadOnly, DiskDriverRef wholeDisk, PartitionDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskInfo info;
    PartitionDriverRef self = NULL;

    try(DiskDriver_GetInfo(wholeDisk, &info));
    if (startBlock >= info.blockCount || blockCount < 1 || (startBlock + blockCount - 1) >= info.blockCount) {
        throw(EINVAL);
    }

    try(DiskDriver_Create(class(PartitionDriver), 0, parent, (DriverRef*)&self));
    self->wholeDisk = wholeDisk;
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

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t PartitionDriver_onStart(PartitionDriverRef _Nonnull _Locked self)
{
    DriverEntry de;
    de.name = self->name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = FilePermissions_MakeFromOctal(0640);
    de.arg = 0;

    return Driver_Publish((DriverRef)self, &de);
}

void PartitionDriver_beginIO(PartitionDriverRef _Nonnull self, DiskRequest* _Nonnull req)
{
    // Update the block number and pass the disk request on to the whole disk
    // driver 
    req->lba += self->startBlock;
    DiskDriver_BeginIO(self->wholeDisk, req);
}


class_func_defs(PartitionDriver, DiskDriver,
override_func_def(onStart, PartitionDriver, Driver)
override_func_def(beginIO, PartitionDriver, DiskDriver)
);
