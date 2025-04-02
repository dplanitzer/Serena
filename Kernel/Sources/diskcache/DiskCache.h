//
//  DiskCache.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskCache_h
#define DiskCache_h

#include <klib/Error.h>
#include <kobj/AnyRefs.h>
#include <driver/disk/DiskRequest.h>
#include <filesystem/FSBlock.h>

enum {
    kDccS_NotRegistered = 0,    // Not registered with the disk cache
    kDccS_Registered,           // Registered with the disk cache
    kDccS_Deregistering         // Deregister called, but still requests in flight. Don't accept any new requested and finalize the deregistration once the last in-flight request is done
};

typedef struct DiskCacheClient {
    int             useCount;
    unsigned int    state;
} DiskCacheClient;


extern DiskCacheRef _Nonnull  gDiskCache;

extern errno_t DiskCache_Create(size_t blockSize, size_t maxBlockCount, DiskCacheRef _Nullable * _Nonnull pOutSelf);

extern errno_t DiskCache_RegisterDisk(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk);
extern void DiskCache_UnregisterDisk(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk);

// Returns the number of bytes that a single block in the disk cache can hold.
extern size_t DiskCache_GetBlockSize(DiskCacheRef _Nonnull self);

extern errno_t DiskCache_PrefetchBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba);
extern errno_t DiskCache_SyncBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba);

// Maps and unmaps a block. Mapping a block allows direct access to the content
// of the disk block.
extern errno_t DiskCache_MapBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba, MapBlock mode, FSBlock* _Nonnull blk);
extern errno_t DiskCache_UnmapBlock(DiskCacheRef _Nonnull self, intptr_t token, WriteBlock mode);

// Pins and unpins the block (disk, media, lba). Pin a block to prevent it from
// being written to disk. A pinned block that is dirty will be retained in
// memory until it is unpinned and a sync of the block is triggered. 
extern errno_t DiskCache_PinBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba);
extern errno_t DiskCache_UnpinBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba);

extern errno_t DiskCache_Sync(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId);

#endif /* DiskCache_h */
