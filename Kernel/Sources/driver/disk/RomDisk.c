//
//  RomDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RomDisk.h"


#define MAX_NAME_LENGTH 8

final_class_ivars(RomDisk, DiskDriver,
    const char* _Nonnull    diskImage;
    LogicalBlockCount       blockCount;
    size_t                  blockSize;
    bool                    freeDiskImageOnClose;
    char                    name[MAX_NAME_LENGTH + 1];
);


errno_t RomDisk_Create(DriverRef _Nullable parent, const char* _Nonnull name, const void* _Nonnull pImage, size_t blockSize, LogicalBlockCount blockCount, bool freeOnClose, RomDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RomDiskRef self = NULL;

    assert(pImage != NULL);
    try(DiskDriver_Create(class(RomDisk), 0, parent, (DriverRef*)&self));
    self->diskImage = pImage;
    self->blockCount = blockCount;
    self->blockSize = blockSize;
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
    return Driver_Publish((DriverRef)self, self->name, kUserId_Root, kGroupId_Root, FilePermissions_MakeFromOctal(0444), 0);
}

errno_t RomDisk_getBlock(RomDiskRef _Nonnull self, const IORequest* _Nonnull ior)
{
    const LogicalBlockAddress lba = ior->address.lba;

    if (lba < self->blockCount) {
        memcpy(DiskBlock_GetMutableData(ior->block), self->diskImage + lba * self->blockSize, self->blockSize);
        return EOK;
    }
    else {
        return EIO;
    }
}


class_func_defs(RomDisk, DiskDriver,
override_func_def(deinit, RomDisk, Object)
override_func_def(onStart, RomDisk, Driver)
override_func_def(getBlock, RomDisk, DiskDriver)
);
