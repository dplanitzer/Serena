//
//  RomDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RomDisk.h"


final_class_ivars(RomDisk, DiskDriver,
    const char* _Nonnull    diskImage;
    LogicalBlockCount       blockCount;
    size_t                  blockSize;
    bool                    freeDiskImageOnClose;
);


errno_t RomDisk_Create(const void* _Nonnull pDiskImage, size_t nBlockSize, LogicalBlockCount nBlockCount, bool freeOnClose, RomDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RomDiskRef self;

    assert(pDiskImage != NULL);
    try(DiskDriver_Create(RomDisk, &self));
    self->diskImage = pDiskImage;
    self->blockCount = nBlockCount;
    self->blockSize = nBlockSize;
    self->freeDiskImageOnClose = freeOnClose;

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void RomDisk_deinit(RomDiskRef _Nonnull self)
{
    if (self->freeDiskImageOnClose) {
        kfree(self->diskImage);
        self->diskImage = NULL;
    }
}

// Returns information about the disk drive and the media loaded into the
// drive.
errno_t RomDisk_getInfoAsync(RomDiskRef _Nonnull self, DiskInfo* pOutInfo)
{
    pOutInfo->blockSize = self->blockSize;
    pOutInfo->blockCount = self->blockCount;
    pOutInfo->mediaId = 1;
    pOutInfo->isReadOnly = true;

    return EOK;
}

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t RomDisk_getBlockAsync(RomDiskRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    if (lba < self->blockCount) {
        memcpy(pBuffer, self->diskImage + lba * self->blockSize, self->blockSize);
        return EOK;
    }
    else {
        return EIO;
    }
}


class_func_defs(RomDisk, DiskDriver,
override_func_def(deinit, RomDisk, Object)
override_func_def(getInfoAsync, RomDisk, DiskDriver)
override_func_def(getBlockAsync, RomDisk, DiskDriver)
);
