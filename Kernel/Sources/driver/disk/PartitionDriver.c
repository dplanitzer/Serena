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
    char                            name[MAX_NAME_LENGTH];
);


errno_t PartitionDriver_Create(DriverRef _Nullable parent, const char* _Nonnull name, LogicalBlockAddress startBlock, LogicalBlockCount blockCount, bool isReadOnly, DiskDriverRef wholeDisk, PartitionDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskInfo diskInfo;
    PartitionDriverRef self = NULL;

    try(DiskDriver_GetInfo(wholeDisk, &diskInfo));
    if (startBlock >= diskInfo.blockCount || blockCount < 1 || (startBlock + blockCount - 1) >= diskInfo.blockCount) {
        throw(EINVAL);
    }

    MediaInfo partInfo;
    partInfo.sectorsPerTrack = 0;   //XXX
    partInfo.heads = 0;             //XXX
    partInfo.cylinders = 0;         //XXX
    partInfo.sectorSize = diskInfo.blockSize;
    partInfo.formatSectorCount = diskInfo.formatSectorCount;
    partInfo.properties = diskInfo.properties;
    if (isReadOnly) {
        partInfo.properties |= kMediaProperty_IsReadOnly;
    }

    try(DiskDriver_Create(class(PartitionDriver), 0, parent, &partInfo, (DriverRef*)&self));
    self->wholeDisk = wholeDisk;
    self->wholeMediaId = diskInfo.mediaId;
    self->startBlock = startBlock;
    self->blockCount = blockCount;
    String_CopyUpTo(self->name, name, MAX_NAME_LENGTH);

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t PartitionDriver_createDispatchQueue(PartitionDriverRef _Nonnull self, DispatchQueueRef _Nullable * _Nonnull pOutQueue)
{
    *pOutQueue = NULL;
    return EOK;
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
    for (size_t i = 0; i < req->rCount; i++) { 
        req->r[i].lba += self->startBlock;
    }
    
    DiskDriver_BeginIO(self->wholeDisk, req);
}

errno_t PartitionDriver_format(PartitionDriverRef _Nonnull _Locked self, FormatSectorsRequest* _Nonnull req)
{
    FormatSectorsRequest req2 = *req;

    req2.addr += self->startBlock;
    return DiskDriver_Format(self->wholeDisk, &req2);
}


class_func_defs(PartitionDriver, DiskDriver,
override_func_def(createDispatchQueue, PartitionDriver, DiskDriver)
override_func_def(onStart, PartitionDriver, Driver)
override_func_def(beginIO, PartitionDriver, DiskDriver)
override_func_def(format, PartitionDriver, DiskDriver)
);
