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
    sno_t                           lsaStart;           // First sector of the partition
    scnt_t                          sectorCount;        // Partition size in terms of sectors
    size_t                          sectorSize;
    char                            name[MAX_NAME_LENGTH];
);


errno_t PartitionDriver_Create(DriverRef _Nullable parent, const char* _Nonnull name, sno_t lsaStart, scnt_t sectorCount, bool isReadOnly, DiskDriverRef wholeDisk, PartitionDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskInfo diskInfo;
    PartitionDriverRef self = NULL;

    try(DiskDriver_GetInfo(wholeDisk, &diskInfo));
    if (lsaStart >= diskInfo.sectorCount || sectorCount < 1 || (lsaStart + sectorCount - 1) >= diskInfo.sectorCount) {
        throw(EINVAL);
    }

    MediaInfo partInfo;
    partInfo.sectorsPerTrack = 0;   //XXX
    partInfo.heads = 0;             //XXX
    partInfo.cylinders = 0;         //XXX
    partInfo.sectorSize = diskInfo.sectorSize;
    partInfo.rwClusterSize = diskInfo.rwClusterSize;
    partInfo.frClusterSize = diskInfo.frClusterSize;
    partInfo.properties = diskInfo.properties;
    if (isReadOnly) {
        partInfo.properties |= kMediaProperty_IsReadOnly;
    }

    try(DiskDriver_Create(class(PartitionDriver), 0, parent, &partInfo, (DriverRef*)&self));
    self->wholeDisk = wholeDisk;
    self->wholeMediaId = diskInfo.mediaId;
    self->lsaStart = lsaStart;
    self->sectorCount = sectorCount;
    self->sectorSize = diskInfo.sectorSize;
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

errno_t PartitionDriver_beginIO(PartitionDriverRef _Nonnull self, IORequest* _Nonnull req)
{
    // Update the sector number and pass the disk request on to the whole disk
    // driver
    //XXX need to take the request type into account
    ((DiskRequest*)req)->offset += self->lsaStart * self->sectorSize;
    
    return DiskDriver_BeginIO(self->wholeDisk, req);
}

errno_t PartitionDriver_doIO(PartitionDriverRef _Nonnull self, IORequest* _Nonnull req)
{
    // Update the sector number and pass the disk request on to the whole disk
    // driver
    //XXX need to take the request type into account
    ((DiskRequest*)req)->offset += self->lsaStart * self->sectorSize;
    
    return DiskDriver_DoIO(self->wholeDisk, req);
}

errno_t PartitionDriver_format(PartitionDriverRef _Nonnull _Locked self, FormatSectorsRequest* _Nonnull req)
{
    FormatSectorsRequest req2 = *req;

    req2.addr += self->lsaStart * self->sectorSize;
    return DiskDriver_Format(self->wholeDisk, &req2);
}


class_func_defs(PartitionDriver, DiskDriver,
override_func_def(createDispatchQueue, PartitionDriver, DiskDriver)
override_func_def(onStart, PartitionDriver, Driver)
override_func_def(beginIO, PartitionDriver, DiskDriver)
override_func_def(doIO, PartitionDriver, DiskDriver)
override_func_def(format, PartitionDriver, DiskDriver)
);
