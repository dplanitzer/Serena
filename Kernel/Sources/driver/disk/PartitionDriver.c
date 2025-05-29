//
//  PartitionDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PartitionDriver.h"
#include <kern/string.h>

#define MAX_NAME_LENGTH 8

// All ivars are protected by the dispatch queue
final_class_ivars(PartitionDriver, DiskDriver,
    DiskDriverRef _Nonnull _Weak    wholeDisk;          // Driver representing the whole disk
    sno_t                           lsaStart;           // First sector of the partition
    scnt_t                          sectorCount;        // Partition size in terms of sectors
    size_t                          sectorSize;
    bool                            isReadOnly;
    char                            name[MAX_NAME_LENGTH];
);


errno_t PartitionDriver_Create(DriverRef _Nullable parent, const char* _Nonnull name, sno_t lsaStart, scnt_t sectorCount, bool isReadOnly, DiskDriverRef wholeDisk, PartitionDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    diskinfo_t diskInfo;
    PartitionDriverRef self = NULL;

    try(DiskDriver_GetInfo(wholeDisk, &diskInfo));
    if (lsaStart >= diskInfo.sectorCount || sectorCount < 1 || (lsaStart + sectorCount - 1) >= diskInfo.sectorCount) {
        throw(EINVAL);
    }

    try(DiskDriver_Create(class(PartitionDriver), 0, parent, (DriverRef*)&self));
    self->wholeDisk = wholeDisk;
    self->lsaStart = lsaStart;
    self->sectorCount = sectorCount;
    self->sectorSize = diskInfo.sectorSize;
    self->isReadOnly = isReadOnly;
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
    decl_try_err();
    diskgeom_t wholeGeom;
    diskinfo_t wholeInfo;

    try(DiskDriver_GetGeometry(self->wholeDisk, &wholeGeom));
    try(DiskDriver_GetInfo(self->wholeDisk, &wholeInfo));

    SensedDisk info;
    info.sectorsPerTrack = wholeGeom.sectorsPerTrack;
    info.heads = wholeGeom.headsPerCylinder;
    info.cylinders = wholeGeom.cylindersPerDisk;
    info.sectorSize = wholeGeom.sectorSize;
    info.rwClusterSize = wholeInfo.rwClusterSize;
    info.properties = wholeInfo.properties;
    if (self->isReadOnly) {
        info.properties |= kDisk_IsReadOnly;
    }
    DiskDriver_NoteSensedDisk((DiskDriverRef)self, &info);


    DriverEntry de;
    de.name = self->name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0640);
    de.arg = 0;

    try(Driver_Publish((DriverRef)self, &de));

catch:
    return err;
}

static DiskDriverRef _Nonnull _prep_req(PartitionDriverRef _Nonnull self, IORequest* _Nonnull r)
{
    switch (r->type) {
        case kDiskRequest_Read:
        case kDiskRequest_Write:
            ((StrategyRequest*)r)->offset += self->lsaStart * self->sectorSize;
            return self->wholeDisk;

        case kDiskRequest_Format:
            ((FormatRequest*)r)->offset += self->lsaStart * self->sectorSize;
            return self->wholeDisk;

        default:
            // Everything else -> just forward it
            return (DiskDriverRef)self;
    }
}

errno_t PartitionDriver_beginIO(PartitionDriverRef _Nonnull self, IORequest* _Nonnull req)
{
    return DiskDriver_BeginIO(_prep_req(self, req), req);
}

errno_t PartitionDriver_doIO(PartitionDriverRef _Nonnull self, IORequest* _Nonnull req)
{
    return DiskDriver_DoIO(_prep_req(self, req), req);
}


class_func_defs(PartitionDriver, DiskDriver,
override_func_def(createDispatchQueue, PartitionDriver, DiskDriver)
override_func_def(onStart, PartitionDriver, Driver)
override_func_def(beginIO, PartitionDriver, DiskDriver)
override_func_def(doIO, PartitionDriver, DiskDriver)
);
