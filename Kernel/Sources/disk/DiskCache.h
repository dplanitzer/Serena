//
//  DiskCache.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskCache_h
#define DiskCache_h

#include <disk/DiskBlock.h>
#include <hal/SystemDescription.h>


extern DiskCacheRef _Nonnull  gDiskCache;

extern errno_t DiskCache_Create(const SystemDescription* _Nonnull pSysDesc, DiskCacheRef _Nullable * _Nonnull pOutSelf);

extern errno_t DiskCache_PrefetchBlock(DiskCacheRef _Nonnull self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba);

extern errno_t DiskCache_AcquireEmptyBlock(DiskCacheRef _Nonnull self, DiskBlockRef _Nullable * _Nonnull pOutBlock);
extern errno_t DiskCache_AcquireBlock(DiskCacheRef _Nonnull self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock);

extern void DiskCache_RelinquishBlock(DiskCacheRef _Nonnull self, DiskBlockRef _Nullable pBlock);
extern errno_t DiskCache_RelinquishBlockWriting(DiskCacheRef _Nonnull self, DiskBlockRef _Nullable pBlock, WriteBlock mode);

extern errno_t DiskCache_Flush(DiskCacheRef _Nonnull self);

extern void DiskCache_OnBlockFinishedIO(DiskCacheRef _Nonnull self, DiskDriverRef pDriver, DiskBlockRef _Nonnull pBlock, errno_t status);

#endif /* DiskCache_h */
