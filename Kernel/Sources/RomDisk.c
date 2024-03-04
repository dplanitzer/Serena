//
//  RomDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RomDisk.h"
#include <System/IOChannel.h>


CLASS_IVARS(RomDisk, DiskDriver,
    const char* _Nonnull    diskImage;
    size_t                  blockCount;
    size_t                  blockSize;
    bool                    freeDiskImageOnClose;
);


errno_t RomDisk_Create(const void* _Nonnull pDiskImage, size_t nBlockCount, size_t nBlockSize, bool freeOnClose, RomDiskRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RomDiskRef self;

    assert(pDiskImage != NULL);
    try(Object_Create(RomDisk, &self));
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

// Returns the size of a block.
size_t RomDisk_getBlockSize(RomDiskRef _Nonnull self)
{
    return self->blockSize;
}

// Returns the number of blocks that the disk is able to store.
size_t RomDisk_getBlockCount(RomDiskRef _Nonnull self)
{
    return self->blockCount;
}

// Reads the contents of the block at index 'idx'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t RomDisk_getBlock(RomDiskRef _Nonnull self, void* _Nonnull pBuffer, size_t idx)
{
    if (idx < self->blockCount) {
        Bytes_CopyRange(pBuffer, self->diskImage + idx * self->blockSize, self->blockSize);
        return EOK;
    }
    else {
        return EIO;
    }
}


CLASS_METHODS(RomDisk, DiskDriver,
OVERRIDE_METHOD_IMPL(deinit, RomDisk, Object)
OVERRIDE_METHOD_IMPL(getBlockSize, RomDisk, DiskDriver)
OVERRIDE_METHOD_IMPL(getBlockCount, RomDisk, DiskDriver)
OVERRIDE_METHOD_IMPL(getBlock, RomDisk, DiskDriver)
);
