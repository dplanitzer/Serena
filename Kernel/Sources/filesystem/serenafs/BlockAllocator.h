//
//  BlockAllocator.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef BlockAllocator_h
#define BlockAllocator_h

#include "VolumeFormat.h"
#include <kobj/Any.h>
#include <dispatcher/Lock.h>


typedef struct BlockAllocator {
    Lock                    lock;                   // Protects all block allocation related state

    uint8_t* _Nullable      bitmap;
    size_t                  bitmapByteSize;
    LogicalBlockAddress     bitmapLba;              // Info for writing the allocation bitmap back to disk
    LogicalBlockCount       bitmapBlockCount;       // -"-

    uint8_t* _Nullable      dirtyBitmapBlocks;      // Each bit represents a block of the bitmap that has changed and needs to be committed to disk

    size_t                  blockSize;              // Disk block size in bytes
    uint32_t                volumeBlockCount;
} BlockAllocator;


extern void BlockAllocator_Init(BlockAllocator* _Nonnull self);
extern void BlockAllocator_Deinit(BlockAllocator* _Nonnull self);

extern errno_t BlockAllocator_Start(BlockAllocator* _Nonnull self, FSContainerRef _Nonnull fsContainer, const SFSVolumeHeader* _Nonnull vhp, size_t blockSize);
extern void BlockAllocator_Stop(BlockAllocator* _Nonnull self);
extern void AllocationBitmap_SetBlockInUse(uint8_t *bitmap, LogicalBlockAddress lba, bool inUse);

extern errno_t BlockAllocator_Allocate(BlockAllocator* _Nonnull self, LogicalBlockAddress* _Nonnull pOutLba);
extern void BlockAllocator_Deallocate(BlockAllocator* _Nonnull self, LogicalBlockAddress lba);

extern errno_t BlockAllocator_CommitToDisk(BlockAllocator* _Nonnull self, FSContainerRef _Nonnull fsContainer);

#endif /* BlockAllocator_h */
