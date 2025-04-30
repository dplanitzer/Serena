//
//  IORequest.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "IORequest.h"
#include <dispatcher/Lock.h>
#include <klib/Kalloc.h>
#include <klib/List.h>
#include <System/_math.h>


#define MAX_CACHED_REQUESTS 8

typedef struct CachedIORequest {
    ListNode    node;
    size_t      size;       // Size of the cached request
} CachedIORequest;


// All 0 by default by virtue of being in the BSS
static Lock gLock;
static List gCache;
static int  gCacheCount;


// Allocates IORequests as a multiple of 16 bytes
errno_t IORequest_Get(int type, size_t reqSize, IORequest* _Nullable * _Nonnull pOutReq)
{
    decl_try_err();
    const size_t targetSize = __Ceil_PowerOf2(reqSize, 16);
    size_t actSize = targetSize;
    IORequest* req = NULL;
    
    assert(targetSize <= UINT16_MAX);

    Lock_Lock(&gLock);

    if (gCacheCount > 0) {
        CachedIORequest* cr = NULL;

        List_ForEach(&gCache, CachedIORequest, {
            if (pCurNode->size >= targetSize) {
                cr = pCurNode;
                break;
            }
        });

        if (cr) {
            List_Remove(&gCache, &cr->node);
            gCacheCount--;

            actSize = cr->size;
            req = (IORequest*)cr;
        }
    }

    Lock_Unlock(&gLock);

    if (req == NULL) {
        err = kalloc(targetSize, (void**)&req);
    }

    if (req) {
        uint32_t* p = (uint32_t*)req;
        size_t nw = targetSize >> 2;

        while (nw-- > 0) {
            *p++ = 0;
        }

        req->type = type;
        req->size = actSize;
    }

    *pOutReq = req;
    return err;
}

void IORequest_Put(IORequest* _Nullable req)
{
    if (req) {
        bool didCache = false;

        Lock_Lock(&gLock);

        if (gCacheCount < MAX_CACHED_REQUESTS) {
            const size_t actSize = req->size;
            CachedIORequest* cr = (CachedIORequest*)req;

            cr->node.next = NULL;
            cr->node.prev = NULL;
            cr->size = actSize;

            List_InsertBeforeFirst(&gCache, &cr->node);
            gCacheCount++;
            didCache = true;
        }

        Lock_Unlock(&gLock);

        if (!didCache) {
            kfree(req);
        }
    }
}
