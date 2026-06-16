//
//  IORequest.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "IORequest.h"
#include <assert.h>
#include <ext/math.h>
#include <ext/queue.h>
#include <kern/kalloc.h>
#include <sched/mtx.h>


#define MAX_CACHED_REQUESTS 8

// All 0 by default by virtue of being in the BSS
static mtx_t    g_mtx;      // BSS which gives us effectively MTX_INIT
static queue_t  g_cache;
static size_t   g_cache_count;


// Allocates IORequests as a multiple of 16 bytes
errno_t IORequest_Get(size_t reqSize, IORequest* _Nullable * _Nonnull pOutReq)
{
    decl_try_err();
    IORequest* iop = NULL;
    
    mtx_lock(&g_mtx);

    if (!queue_empty(&g_cache)) {
        IORequest* prev_iop = NULL;

        queue_for_each(&g_cache, IORequest, it,
            if (it->size >= reqSize) {
                queue_remove(&g_cache, (queue_node_t*)prev_iop, &it->u.qe);
                g_cache_count--;
                iop = it;
                break;
            }

            prev_iop = it;
        );
    }

    mtx_unlock(&g_mtx);


    if (iop == NULL) {
        const size_t bucketSize = __Ceil_PowerOf2(reqSize, 16);

        err = kalloc(bucketSize, (void**)&iop);
        if (err == EOK) {
            iop->size = bucketSize;
        }
    }


    *pOutReq = iop;
    return err;
}

void IORequest_Put(IORequest* _Nullable iop)
{
    if (iop) {
        bool didCache = false;

        mtx_lock(&g_mtx);

        if (g_cache_count < MAX_CACHED_REQUESTS) {
            _queue_add_first(&g_cache, &iop->u.qe);
            g_cache_count++;
            didCache = true;
        }

        mtx_unlock(&g_mtx);


        if (!didCache) {
            kfree(iop);
        }
    }
}
