//
//  DiskCache.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskCache_h
#define DiskCache_h

#include <kern/errno.h>
#include <kobj/AnyRefs.h>
#include <filesystem/FSBlock.h>
#include <filesystem/IOChannel.h>
#include <kpi/disk.h>


typedef struct DiskSession {
    IOChannelRef _Nullable  channel;
    DiskDriverRef _Nullable disk;
    int                     sessionId;
    size_t                  sectorSize;
    size_t                  s2bFactor;
    size_t                  trailPadSize;
    scnt_t                  rwClusterSize;
    int                     activeMappingsCount;
    bool                    isOpen;
} DiskSession;


extern DiskCacheRef _Nonnull  gDiskCache;

extern errno_t DiskCache_Create(size_t blockSize, size_t maxBlockCount, DiskCacheRef _Nullable * _Nonnull pOutSelf);

// Returns the number of bytes that a single block in the disk cache can hold.
extern size_t DiskCache_GetBlockSize(DiskCacheRef _Nonnull self);


// Opens a new disk cache session. The session will be backed by the given disk
// and the session will automatically map a disk cache (logical) block to one or
// more disk sectors. If the logical block size is evenly divisible by the
// sector size then multiple sectors will be packed into a single logical block.
// If on the other hand the sector size is not a multiple of the logical block
// size (eg CD-ROM sector size: 2,352 bytes) then a single sector will be mapped
// to a single logical block. The remaining bytes will be ignored on write and
// filled with zeros on read.  
extern void DiskCache_OpenSession(DiskCacheRef _Nonnull self, IOChannelRef _Nonnull chan, const disk_info_t* _Nonnull info, DiskSession* _Nonnull s);
extern void DiskCache_CloseSession(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s);

extern errno_t DiskCache_PrefetchBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, blkno_t lba);
extern errno_t DiskCache_SyncBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, blkno_t lba);

// Maps and unmaps a block. Mapping a block allows direct access to the content
// of the disk block.
extern errno_t DiskCache_MapBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, blkno_t lba, MapBlock mode, FSBlock* _Nonnull blk);
extern errno_t DiskCache_UnmapBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, intptr_t token, WriteBlock mode);

// Pins and unpins the block (disk, media, lba). Pin a block to prevent it from
// being written to disk. A pinned block that is dirty will be retained in
// memory until it is unpinned and a sync of the block is triggered. 
extern errno_t DiskCache_PinBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, blkno_t lba);
extern errno_t DiskCache_UnpinBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, blkno_t lba);

extern errno_t DiskCache_Sync(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s);

#endif /* DiskCache_h */
