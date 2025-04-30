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
    size_t      iovCapacity;    // Number of block request slots in this cached disk request
} CachedDiskRequest;


// All 0 by default by virtue of being in the BSS
static Lock gLock;
static List gCache;
static int  gCacheCount;


errno_t DiskRequest_Get(size_t iovCapacity, DiskRequest* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskRequest* self = NULL;
    
    Lock_Lock(&gLock);

    if (gCacheCount > 0) {
        CachedDiskRequest* cdr = NULL;

        List_ForEach(&gCache, CachedDiskRequest, {
            if (pCurNode->iovCapacity >= iovCapacity) {
                cdr = pCurNode;
                break;
            }
        });

        if (cdr) {
            List_Remove(&gCache, &cdr->node);
            gCacheCount--;

            const size_t capacity = cdr->iovCapacity;
            self = (DiskRequest*)cdr;
            self->iovCapacity = capacity;
        }
    }

    Lock_Unlock(&gLock);

    if (self == NULL) {
        err = kalloc(sizeof(DiskRequest) + sizeof(IOVector) * (iovCapacity - 1), (void**)&self);
        if (err == EOK) {
            self->iovCapacity = iovCapacity;
        }
    }

    if (self) {
        self->done = NULL;
        self->context = NULL;
        self->type = 0;
        self->iovCount = 0;
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
            const size_t capacity = self->iovCapacity;
            CachedDiskRequest* cdr = (CachedDiskRequest*)self;

            cdr->node.next = NULL;
            cdr->node.prev = NULL;
            cdr->iovCapacity = capacity;

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

void DiskRequest_Done(DiskRequest* _Nonnull self, IOVector* _Nullable iov, errno_t status)
{
    if (self->done) {
        self->done(self->context, self, iov, status);
    }
}
