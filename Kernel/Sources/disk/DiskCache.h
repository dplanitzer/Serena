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
#include <filesystem/FSBlock.h>
#include <hal/SystemDescription.h>

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

extern errno_t DiskCache_Create(const SystemDescription* _Nonnull pSysDesc, DiskCacheRef _Nullable * _Nonnull pOutSelf);

extern errno_t DiskCache_PrefetchBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba);
extern errno_t DiskCache_SyncBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba);

extern errno_t DiskCache_MapEmptyBlock(DiskCacheRef _Nonnull self, FSBlock* _Nonnull blk);
extern errno_t DiskCache_MapBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba, MapBlock mode, FSBlock* _Nonnull blk);

extern void DiskCache_UnmapBlock(DiskCacheRef _Nonnull self, intptr_t token);
extern errno_t DiskCache_UnmapBlockWriting(DiskCacheRef _Nonnull self, intptr_t token, WriteBlock mode);

extern errno_t DiskCache_Sync(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId);

extern void DiskCache_OnBlockFinishedIO(DiskCacheRef _Nonnull self, DiskBlockRef _Nonnull pBlock, errno_t status);

extern errno_t DiskCache_RegisterDisk(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk);
extern void DiskCache_UnregisterDisk(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk);

#endif /* DiskCache_h */
