//
//  RomDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RomDisk.h"
#include <diskcache/DiskCache.h>


#define MAX_NAME_LENGTH 8

final_class_ivars(RomDisk, DiskDriver,
    const char* _Nonnull    diskImage;
    LogicalBlockCount       blockCount;
    size_t                  blockSize;
    size_t                  blockShift;
    bool                    freeDiskImageOnClose;
    char                    name[MAX_NAME_LENGTH + 1];
);


errno_t RomDisk_Create(DriverRef _Nullable parent, const char* _Nonnull name, const void* _Nonnull pImage, size_t blockSize, LogicalBlockCount blockCount, bool freeOnClose, RomDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RomDiskRef self = NULL;

    if (pImage == NULL || !siz_ispow2(blockSize)) {
        throw(EINVAL);
    }

    try(DiskDriver_Create(class(RomDisk), 0, parent, gDiskCache, (DriverRef*)&self));
    self->diskImage = pImage;
    self->blockCount = blockCount;
    self->blockSize = blockSize;
    self->blockShift = siz_log2(self->blockSize);
    self->freeDiskImageOnClose = freeOnClose;
    String_CopyUpTo(self->name, name, MAX_NAME_LENGTH);

    MediaInfo info;
    info.blockCount = self->blockCount;
    info.blockSize = self->blockSize;
    info.isReadOnly = true;
    DiskDriver_NoteMediaLoaded((DiskDriverRef)self, &info);

catch:
    *pOutSelf = self;
    return err;
}

void RomDisk_deinit(RomDiskRef _Nonnull self)
{
    if (self->freeDiskImageOnClose) {
        kfree(self->diskImage);
        self->diskImage = NULL;
    }
}

errno_t RomDisk_onStart(RomDiskRef _Nonnull _Locked self)
{
    DriverEntry de;
    de.name = self->name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = FilePermissions_MakeFromOctal(0444);
    de.arg = 0;

    return Driver_Publish((DriverRef)self, &de);
}

errno_t RomDisk_getMediaBlock(RomDiskRef _Nonnull self, LogicalBlockAddress ba, uint8_t* _Nonnull data)
{
    if (ba < self->blockCount) {
        memcpy(data, self->diskImage + (ba << self->blockShift), self->blockSize);
        return EOK;
    }
    else {
        return ENXIO;
    }
}


class_func_defs(RomDisk, DiskDriver,
override_func_def(deinit, RomDisk, Object)
override_func_def(onStart, RomDisk, Driver)
override_func_def(getMediaBlock, RomDisk, DiskDriver)
);
