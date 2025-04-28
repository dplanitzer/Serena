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

struct SectorRequest;
struct DiskRequest;


typedef (*DiskRequestDoneCallback)(void* ctx, struct DiskRequest* _Nonnull req, struct SectorRequest* _Nullable br, errno_t status);


enum {
    kDiskRequest_Read = 1,
    kDiskRequest_Write = 2
};


typedef struct SectorRequest {
    size_t                  offset;     // -> logical sector address in terms of bytes
    ssize_t                 size;       // -> request size in terms of bytes
    uint8_t* _Nonnull       data;       // -> byte buffer to read or write 
    intptr_t                token;      // -> token identifying this disk block range
} SectorRequest;


typedef struct DiskRequest {
    DiskRequestDoneCallback _Nullable   done;           // -> done callback
    void* _Nullable                     context;        // -> done callback context
    intptr_t                            refCon;         // -> user-defined value
    int                                 type;           // -> disk request type: read/write
    MediaId                             mediaId;        // -> disk media identity
    size_t                              rCapacity;      // -> number of block ranges the request can hold
    size_t                              rCount;         // -> number of block ranges that are actually set up in the request

    SectorRequest                       r[1];
} DiskRequest;


extern errno_t DiskRequest_Get(size_t rCapacity, DiskRequest* _Nullable * _Nonnull pOutSelf);
extern void DiskRequest_Put(DiskRequest* _Nullable self);

// Call this to mark the request as done. This will synchronously invoke the
// request's done callback.
extern void DiskRequest_Done(DiskRequest* _Nonnull self, SectorRequest* _Nullable br, errno_t status);

#endif /* DiskRequest_h */
