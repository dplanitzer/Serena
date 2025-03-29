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
    SListNode   node;
} CachedDiskRequest;


// All 0 by default by virtue of being in the BSS
static Lock     gLock;
static SList    gCache;
static int      gCacheCount;


errno_t DiskRequest_Get(DiskRequest* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskRequest* self = NULL;
    
    Lock_Lock(&gLock);

    if (gCacheCount > 0) {
        self = (DiskRequest*)SList_RemoveFirst(&gCache);
        gCacheCount--;
    }

    if (self == NULL) {
        try(kalloc_cleared(sizeof(DiskRequest), (void**)&self));
    }

catch:
    Lock_Unlock(&gLock);

    *pOutSelf = self;
    return err;
}

void DiskRequest_Put(DiskRequest* _Nullable self)
{
    if (self) {
        Lock_Lock(&gLock);

        if (gCacheCount < MAX_CACHED_REQUESTS) {
            CachedDiskRequest* cdr = (CachedDiskRequest*)self;

            SList_InsertBeforeFirst(&gCache, &cdr->node);
            gCacheCount++;
        }
        else {
            kfree(self);
        }

        Lock_Unlock(&gLock);
    }
}
