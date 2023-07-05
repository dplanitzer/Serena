//
//  VirtualProcessorPool.c
//  Apollo
//
//  Created by Dietmar Planitzer on 4/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VirtualProcessorPool.h"
#include "Heap.h"
#include "Lock.h"
#include "SystemGlobals.h"
#include "VirtualProcessorScheduler.h"


#define REUSE_CACHE_CAPACITY    16
typedef struct _VirtualProcessorPool {
    Lock    lock;
    List    inuse_queue;        // VPs in use
    List    reuse_queue;        // VPs available for reuse
    Int     inuse_count;        // count of VPs that are in use
    Int     reuse_count;        // count of how many VPs are in the reuse queue
    Int     reuse_capacity;     // reuse cache will not store more than this. If a VP exits while the cache is at max capacity -> VP will exit for good and get finalized
} VirtualProcessorPool;


// Returns the shared virtual processor pool.
VirtualProcessorPoolRef _Nonnull VirtualProcessorPool_GetShared(void)
{
    return SystemGlobals_Get()->virtual_processor_pool;
}

VirtualProcessorPoolRef _Nullable VirtualProcessorPool_Create(void)
{
    VirtualProcessorPool* pool = (VirtualProcessorPool*)kalloc(sizeof(VirtualProcessorPool), 0);
    FailNULL(pool);
    
    List_Init(&pool->inuse_queue);
    List_Init(&pool->reuse_queue);
    Lock_Init(&pool->lock);
    pool->inuse_count = 0;
    pool->reuse_count = 0;
    pool->reuse_capacity = REUSE_CACHE_CAPACITY;
    return pool;
    
failed:
    VirtualProcessorPool_Destroy(pool);
    return NULL;
}

void VirtualProcessorPool_Destroy(VirtualProcessorPoolRef _Nullable pool)
{
    if (pool) {
        List_Deinit(&pool->inuse_queue);
        List_Deinit(&pool->reuse_queue);
        Lock_Deinit(&pool->lock);
        kfree((Byte*)pool);
    }
}

VirtualProcessor* _Nonnull VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPoolRef _Nonnull pool, VirtualProcessorParameters params)
{
    VirtualProcessor* pVP = NULL;

    Lock_Lock(&pool->lock);

    // Try to reuse a cached VP
    VirtualProcessorOwner* pCurVP = (VirtualProcessorOwner*)pool->reuse_queue.first;
    while (pCurVP) {
        if (VirtualProcessor_IsSuspended(pCurVP->self)) {
            pVP = pCurVP->self;
            break;
        }
        pCurVP = (VirtualProcessorOwner*)pCurVP->queue_entry.next;
    }
        
    if (pVP) {
        List_Remove(&pool->reuse_queue, &pVP->owner.queue_entry);
        pool->reuse_count--;
        
        List_InsertBeforeFirst(&pool->inuse_queue, &pVP->owner.queue_entry);
        pool->inuse_count++;
    }
    
    Lock_Unlock(&pool->lock);
    
    
    // Create a new VP if we were not able to reuse a cached one
    if (pVP == NULL) {
        pVP = VirtualProcessor_Create();
        FailNULL(pVP);

        Lock_Lock(&pool->lock);
        List_InsertBeforeFirst(&pool->inuse_queue, &pVP->owner.queue_entry);
        pool->inuse_count++;
        Lock_Unlock(&pool->lock);
    }
    
    
    // Configure the VP
    VirtualProcessor_SetPriority(pVP, params.priority);
    FailErr(VirtualProcessor_SetClosure(pVP, VirtualProcessorClosure_Make(params.func, params.context, params.kernelStackSize, params.userStackSize)));

    return pVP;

failed:
    return NULL;
}

// Relinquishes the given VP back to the reuse pool if possible. If the reuse
// pool is full then the given VP is suspended and scheduled for finalization
// instead. Note that the VP is suspended in any case.
_Noreturn VirtualProcessorPool_RelinquishVirtualProcessor(VirtualProcessorPoolRef _Nonnull pool, VirtualProcessor* _Nonnull pVP)
{
    Bool didReuse = false;

    // Null out the dispatch queue reference in any case since the VP should no
    // longer be associated with a queue.
    VirtualProcessor_SetDispatchQueue(pVP, NULL, -1);


    // Try to cache the VP
    Lock_Lock(&pool->lock);
    
    List_Remove(&pool->inuse_queue, &pVP->owner.queue_entry);
    pool->inuse_count--;

    if (pool->reuse_count < pool->reuse_capacity) {
        List_InsertBeforeFirst(&pool->reuse_queue, &pVP->owner.queue_entry);
        pool->reuse_count++;
        didReuse = true;
    }
    Lock_Unlock(&pool->lock);
    

    // Suspend the VP if we decided to reuse it and schedule it for finalization
    // (termination) otherwise.
    if (didReuse) {
        VirtualProcessor_Suspend(pVP);
    } else {
        VirtualProcessor_Terminate(pVP);
    }
    /* NOT REACHED */
}
