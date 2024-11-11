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


errno_t RomDisk_Create(const char* _Nonnull name, const void* _Nonnull pImage, size_t blockSize, LogicalBlockCount blockCount, bool freeOnClose, RomDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RomDiskRef self = NULL;

    assert(pImage != NULL);
    try(DiskDriver_Create(RomDisk, &self));
    self->diskImage = pImage;
    self->blockCount = blockCount;
    self->blockSize = blockSize;
    self->freeDiskImageOnClose = freeOnClose;
    String_CopyUpTo(self->name, name, MAX_NAME_LENGTH);

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

errno_t RomDisk_start(RomDiskRef _Nonnull self)
{
    return Driver_Publish((DriverRef)self, self->name);
}

errno_t RomDisk_getInfo_async(RomDiskRef _Nonnull self, DiskInfo* pOutInfo)
{
    pOutInfo->blockSize = self->blockSize;
    pOutInfo->blockCount = self->blockCount;
    pOutInfo->mediaId = 1;
    pOutInfo->isReadOnly = true;

    return EOK;
}

MediaId RomDisk_getCurrentMediaId(RomDiskRef _Nonnull self)
{
    return 1;
}

errno_t RomDisk_getBlock(RomDiskRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    const LogicalBlockAddress lba = DiskBlock_GetLba(pBlock);

    if (lba < self->blockCount) {
        memcpy(DiskBlock_GetMutableData(pBlock), self->diskImage + lba * self->blockSize, self->blockSize);
        return EOK;
    }
    else {
        return EIO;
    }
}


class_func_defs(RomDisk, DiskDriver,
override_func_def(deinit, RomDisk, Object)
override_func_def(start, RomDisk, Driver)
override_func_def(getInfo_async, RomDisk, DiskDriver)
override_func_def(getCurrentMediaId, RomDisk, DiskDriver)
override_func_def(getBlock, RomDisk, DiskDriver)
);
