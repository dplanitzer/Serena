//
//  FauxDisk.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FauxDisk.h"


errno_t DiskDriver_Create(size_t nBlockSize, LogicalBlockCount nBlockCount, DiskDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskDriverRef self = calloc(1, sizeof(DiskDriver));

    self->disk = calloc(nBlockCount, nBlockSize);
    self->blockSize = nBlockSize;
    self->blockCount = nBlockCount;

    *pOutSelf = self;
    return EOK;

catch:
    DiskDriver_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void DiskDriver_Destroy(DiskDriverRef _Nullable self)
{
    if (self) {
        free(self->disk);
        self->disk = NULL;
        free(self);
    }
}

// Returns the size of a block.
size_t DiskDriver_GetBlockSize(DiskDriverRef _Nonnull self)
{
    return self->blockSize;
}

// Returns the number of blocks that the disk is able to store.
LogicalBlockCount DiskDriver_GetBlockCount(DiskDriverRef _Nonnull self)
{
    return self->blockCount;
}

// Returns true if the disk if read-only.
bool DiskDriver_IsReadOnly(DiskDriverRef _Nonnull self)
{
    return false;
}

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t DiskDriver_GetBlock(DiskDriverRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    if (lba >= self->blockCount) {
        return EIO;
    }

    memcpy(pBuffer, &self->disk[lba * self->blockSize], self->blockSize);
    return EOK;
}

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
errno_t DiskDriver_PutBlock(DiskDriverRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    if (lba >= self->blockCount) {
        return EIO;
    }

    memcpy(&self->disk[lba * self->blockSize], pBuffer, self->blockSize);
    return EOK;
}

// Writes the contents of the disk to the given path as a regular file.
errno_t DiskDriver_WriteToPath(DiskDriverRef _Nonnull self, const char* pPath)
{
    decl_try_err();
    FILE* fp;

    try_null(fp, fopen(pPath, "wb"), EIO);
    const size_t nwritten = fwrite(self->disk, self->blockSize, self->blockCount, fp);
    if (nwritten < self->blockCount) {
        throw(EIO);
    }

catch:
    fclose(fp);
    return err;
}
