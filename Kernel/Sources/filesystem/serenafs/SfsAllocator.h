//
//  SfsAllocator.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef SfsAllocator_h
#define SfsAllocator_h

#include <kobj/Any.h>
#include <dispatcher/Lock.h>
#include "VolumeFormat.h"


typedef struct SfsAllocator {
    Lock                    lock;                   // Protects all block allocation related state

    uint8_t* _Nullable      bitmap;
    size_t                  bitmapByteSize;
    bno_t                   bitmapLba;              // Info for writing the allocation bitmap back to disk
    bcnt_t                  bitmapBlockCount;       // -"-

    uint8_t* _Nullable      dirtyBitmapBlocks;      // Each bit represents a block of the bitmap that has changed and needs to be committed to disk

    size_t                  blockSize;              // Disk block size in bytes
    uint32_t                volumeBlockCount;
} SfsAllocator;


extern void SfsAllocator_Init(SfsAllocator* _Nonnull self);
extern void SfsAllocator_Deinit(SfsAllocator* _Nonnull self);

extern errno_t SfsAllocator_Start(SfsAllocator* _Nonnull self, FSContainerRef _Nonnull fsContainer, const sfs_vol_header_t* _Nonnull vhp, size_t blockSize);
extern void SfsAllocator_Stop(SfsAllocator* _Nonnull self);
extern void AllocationBitmap_SetBlockInUse(uint8_t *bitmap, bno_t lba, bool inUse);

extern errno_t SfsAllocator_Allocate(SfsAllocator* _Nonnull self, bno_t* _Nonnull pOutLba);
extern void SfsAllocator_Deallocate(SfsAllocator* _Nonnull self, bno_t lba);

extern errno_t SfsAllocator_CommitToDisk(SfsAllocator* _Nonnull self, FSContainerRef _Nonnull fsContainer);

extern bcnt_t SfsAllocator_GetAllocatedBlockCount(SfsAllocator* _Nonnull self);

#endif /* SfsAllocator_h */
