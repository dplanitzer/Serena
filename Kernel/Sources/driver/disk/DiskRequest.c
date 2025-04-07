//
//  DiskRequest.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DiskRequest.h"
#include <dispatcher/Lock.h>
#include <klib/Kalloc.h>
#include <klib/List.h>


#define MAX_CACHED_REQUESTS 4

typedef struct CachedDiskRequest {
    ListNode    node;
    size_t      rCapacity;  // Number of block request slots in this cached disk request
} CachedDiskRequest;


// All 0 by default by virtue of being in the BSS
static Lock gLock;
static List gCache;
static int  gCacheCount;


errno_t DiskRequest_Get(size_t rCapacity, DiskRequest* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskRequest* self = NULL;
    
    Lock_Lock(&gLock);

    if (gCacheCount > 0) {
        CachedDiskRequest* cdr = NULL;

        List_ForEach(&gCache, CachedDiskRequest, {
            if (pCurNode->rCapacity >= rCapacity) {
                cdr = pCurNode;
                break;
            }
        });

        if (cdr) {
            List_Remove(&gCache, &cdr->node);
            gCacheCount--;

            const size_t capacity = cdr->rCapacity;
            self = (DiskRequest*)cdr;
            self->rCapacity = capacity;
        }
    }

    Lock_Unlock(&gLock);

    if (self == NULL) {
        err = kalloc(sizeof(DiskRequest) + sizeof(BlockRequest) * (rCapacity - 1), (void**)&self);
        if (err == EOK) {
            self->rCapacity = rCapacity;
        }
    }

    if (self) {
        self->done = NULL;
        self->context = NULL;
        self->type = 0;
        self->rCount = 0;
    }

    *pOutSelf = self;
    return err;
}

void DiskRequest_Put(DiskRequest* _Nullable self)
{
    if (self) {
        bool didCache = false;

        Lock_Lock(&gLock);

        if (gCacheCount < MAX_CACHED_REQUESTS) {
            const size_t capacity = self->rCapacity;
            CachedDiskRequest* cdr = (CachedDiskRequest*)self;

            cdr->node.next = NULL;
            cdr->node.prev = NULL;
            cdr->rCapacity = capacity;

            List_InsertBeforeFirst(&gCache, &cdr->node);
            gCacheCount++;
            didCache = true;
        }

        Lock_Unlock(&gLock);

        if (!didCache) {
            kfree(self);
        }
    }
}

void DiskRequest_Done(DiskRequest* _Nonnull self, BlockRequest* _Nullable br, errno_t status)
{
    if (self->done) {
        self->done(self->context, self, br, status);
    }
}
