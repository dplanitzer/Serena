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

struct DiskRequest;


typedef (*DiskRequestDoneCallback)(void* ctx, struct DiskRequest* _Nonnull req);


typedef struct DiskRequest {
    DiskRequestDoneCallback _Nullable   done;       // <- done callback
    void* _Nullable                     context;    // <- done callback context
    errno_t                             status;     // <- final disk request status

    MediaId                 mediaId;    // -> physical disk block address
    LogicalBlockAddress     lba;
    DiskBlockRef _Nonnull   block;      // -> disk block to read/write
} DiskRequest;


extern errno_t DiskRequest_Get(DiskRequest* _Nullable * _Nonnull pOutSelf);
extern void DiskRequest_Put(DiskRequest* _Nullable self);

// Call this to mark the request as done. This will synchronously invoke the
// request's done callback.
extern void DiskRequest_Done(DiskRequest* _Nonnull self, errno_t status);

#endif /* DiskRequest_h */
