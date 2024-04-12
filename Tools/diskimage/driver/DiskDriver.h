//
//  DiskDriver.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_DiskDriver_h
#define di_DiskDriver_h

#include <klib/Error.h>
#include <klib/Types.h>
#include <kobj/Object.h>


// Represents a logical block address in the range 0..<DiskDriver.blockCount
typedef uint32_t    LogicalBlockAddress;

// Type to represent the number of blocks on a disk
typedef LogicalBlockAddress LogicalBlockCount;


open_class_with_ref(DiskDriver, Object,
    uint8_t*            disk;
    size_t              blockSize;
    LogicalBlockCount   blockCount;
);
typedef struct DiskDriverMethodTable {
    ObjectMethodTable   super;
} DiskDriverMethodTable;




extern errno_t DiskDriver_Create(size_t nBlockSize, LogicalBlockCount nBlockCount, DiskDriverRef _Nullable * _Nonnull pOutSelf);
extern void DiskDriver_Destroy(DiskDriverRef _Nullable self);

// Returns the size of a block.
// The abstract implementation returns 0.
extern size_t DiskDriver_GetBlockSize(DiskDriverRef _Nonnull self);

// Returns the number of blocks that the disk is able to store.
// The abstract implementation returns 0.
extern LogicalBlockCount DiskDriver_GetBlockCount(DiskDriverRef _Nonnull self);

// Returns true if the disk if read-only.
// The abstract implementation returns true.
extern bool DiskDriver_IsReadOnly(DiskDriverRef _Nonnull self);

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
// The abstract implementation returns EIO.
extern errno_t DiskDriver_GetBlock(DiskDriverRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba);

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
// The abstract implementation returns EIO.
extern errno_t DiskDriver_PutBlock(DiskDriverRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba);

// Writes the contents of the disk to the given path as a regular file.
extern errno_t DiskDriver_WriteToPath(DiskDriverRef _Nonnull self, const char* pPath);

#endif /* di_DiskDriver_h */
