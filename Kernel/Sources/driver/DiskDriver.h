//
//  DiskDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskDriver_h
#define DiskDriver_h

#include <klib/klib.h>


// Represents a logical block address in the range 0..<DiskDriver.blockCount
typedef uint32_t    LogicalBlockAddress;

// Type to represent the number of blocks on a disk
typedef LogicalBlockAddress LogicalBlockCount;


// A disk driver manages the data stored on a disk. It provides read and write
// access to the disk data. Data on a disk is organized in blocks. All blocks
// are of the same size. Blocks are addresses with an index in the range
// [0, BlockCount].
OPEN_CLASS_WITH_REF(DiskDriver, Object,
);
typedef struct _DiskDriverMethodTable {
    ObjectMethodTable   super;

    // Returns the size of a block.
    // The abstract implementation returns 0.
    size_t (*getBlockSize)(void* _Nonnull self);

    // Returns the number of blocks that the disk is able to store.
    // The abstract implementation returns 0.
    LogicalBlockCount (*getBlockCount)(void* _Nonnull self);

    // Returns true if the disk if read-only.
    // The abstract implementation returns true.
    bool (*isReadOnly)(void* _Nonnull self);

    // Reads the contents of the block at index 'lba'. 'buffer' must be big
    // enough to hold the data of a block. Blocks the caller until the read
    // operation has completed. Note that this function will never return a
    // partially read block. Either it succeeds and the full block data is
    // returned, or it fails and no block data is returned.
    // The abstract implementation returns EIO.
    errno_t (*getBlock)(void* _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba);

    // Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
    // must be big enough to hold a full block. Blocks the caller until the
    // write has completed. The contents of the block on disk is left in an
    // indeterminate state of the write fails in the middle of the write. The
    // block may contain a mix of old and new data.
    // The abstract implementation returns EIO.
    errno_t (*putBlock)(void* _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba);

} DiskDriverMethodTable;


//
// Methods for use by disk driver users.
//

#define DiskDriver_GetBlockSize(__self) \
Object_Invoke0(getBlockSize, DiskDriver, __self)

#define DiskDriver_GetBlockCount(__self) \
Object_Invoke0(getBlockCount, DiskDriver, __self)

#define DiskDriver_IsReadOnly(__self) \
Object_Invoke0(isReadOnly, DiskDriver, __self)

#define DiskDriver_GetBlock(__self, __pBuffer, __lba) \
Object_InvokeN(getBlock, DiskDriver, __self, __pBuffer, __lba)

#define DiskDriver_PutBlock(__self, __pBuffer, __lba) \
Object_InvokeN(putBlock, DiskDriver, __self, __pBuffer, __lba)

#endif /* DiskDriver_h */
