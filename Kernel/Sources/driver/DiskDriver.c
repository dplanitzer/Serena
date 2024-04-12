//
//  DiskDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriver.h"


// Returns the size of a block.
size_t DiskDriver_getBlockSize(DiskDriverRef _Nonnull self)
{
    return 0;
}

// Returns the number of blocks that the disk is able to store.
LogicalBlockCount DiskDriver_getBlockCount(DiskDriverRef _Nonnull self)
{
    return 0;
}

// Returns true if the disk if read-only.
bool DiskDriver_isReadOnly(DiskDriverRef _Nonnull self)
{
    return true;
}

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t DiskDriver_getBlock(DiskDriverRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    return EIO;
}

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
errno_t DiskDriver_putBlock(DiskDriverRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    return EIO;
}


class_func_defs(DiskDriver, Object,
func_def(getBlockSize, DiskDriver)
func_def(getBlockCount, DiskDriver)
func_def(isReadOnly, DiskDriver)
func_def(getBlock, DiskDriver)
func_def(putBlock, DiskDriver)
);
