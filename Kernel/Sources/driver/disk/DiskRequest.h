//
//  DiskRequest.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskRequest_h
#define DiskRequest_h

#include <klib/Error.h>
#include <klib/Types.h>
#include <kobj/AnyRefs.h>


typedef struct DiskRequest {
    MediaId                 mediaId;    // Physical disk block address
    LogicalBlockAddress     lba;
    DiskBlockRef _Nonnull   block;      // Disk block to read/write
} DiskRequest;


extern errno_t DiskRequest_Get(DiskRequest* _Nullable * _Nonnull pOutSelf);
extern void DiskRequest_Put(DiskRequest* _Nullable self);

#endif /* DiskRequest_h */
