//
//  VirtualProcessorPool.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VirtualProcessorPool.h"
#include <kern/kalloc.h>
#include <sched/mtx.h>
#include <sched/sched.h>


#define REUSE_CACHE_CAPACITY    16
typedef struct VirtualProcessorPool {
    mtx_t   mtx;
    List    reuse_queue;        // VPs available for reuse
    int     reuse_count;        // count of how many VPs are in the reuse queue
    int     reuse_capacity;     // reuse cache will not store more than this. If a VP exits while the cache is at max capacity -> VP will exit for good and get finalized
} VirtualProcessorPool;


VirtualProcessorPoolRef gVirtualProcessorPool;


errno_t VirtualProcessorPool_Create(VirtualProcessorPoolRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    VirtualProcessorPool* self;
    
    try(kalloc_cleared(sizeof(VirtualProcessorPool), (void**) &self));
    List_Init(&self->reuse_queue);
    mtx_init(&self->mtx);
    self->reuse_capacity = REUSE_CACHE_CAPACITY;
    *pOutSelf = self;
    return EOK;
    
catch:
    VirtualProcessorPool_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void VirtualProcessorPool_Destroy(VirtualProcessorPoolRef _Nullable self)
{
    if (self) {
        List_Deinit(&self->reuse_queue);
        mtx_deinit(&self->mtx);
        kfree(self);
    }
}

errno_t VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPoolRef _Nonnull self, const VirtualProcessorParameters* _Nonnull params, vcpu_t _Nonnull * _Nonnull pOutVP)
{
    decl_try_err();
    vcpu_t vp = NULL;

    mtx_lock(&self->mtx);

    // Try to reuse a cached VP
    List_ForEach(&self->reuse_queue, ListNode, {
        vcpu_t cvp = VP_FROM_OWNER_NODE(pCurNode);

        // Make sure that the VP is suspended at this point. It may still be in
        // the process of finishing the suspend. See relinquish() below
        if (vcpu_suspended(cvp)) {
            vp = cvp;
            break;
        }
    });
        
    if (vp) {
        List_Remove(&self->reuse_queue, &vp->owner_qe);
        self->reuse_count--;
    }
    
    mtx_unlock(&self->mtx);
    
    
    // Create a new VP if we were not able to reuse a cached one
    if (vp == NULL) {
        try(vcpu_create(&vp));
    }
    
    
    // Configure the VP
    VirtualProcessorClosure cl;
    cl.func = (VoidFunc_1)params->func;
    cl.context = params->context;
    cl.ret_func = params->ret_func;
    cl.kernelStackBase = NULL;
    cl.kernelStackSize = params->kernelStackSize;
    cl.userStackSize = params->userStackSize;
    cl.isUser = params->isUser;

    try(vcpu_setclosure(vp, &cl));
    vcpu_setpriority(vp, params->priority);
    if (params->isUser) {
        vp->flags |= VP_FLAG_USER_OWNED;
    }
    else {
        vp->flags &= ~VP_FLAG_USER_OWNED;
    }
    vp->vpgid = params->vpgid;
    vp->lifecycle_state = VP_LIFECYCLE_ACQUIRED;

catch:
    *pOutVP = vp;
    return err;
}

// Relinquishes the given VP back to the reuse pool if possible. If the reuse
// pool is full then the given VP is suspended and scheduled for finalization
// instead. Note that the VP is suspended in any case.
_Noreturn VirtualProcessorPool_RelinquishVirtualProcessor(VirtualProcessorPoolRef _Nonnull self, vcpu_t _Nonnull vp)
{
    bool doReuse = false;

    // Null out the dispatch queue reference in any case since the VP should no
    // longer be associated with a queue.
    vcpu_setdq(vp, NULL, -1);


    // Try to cache the VP
    mtx_lock(&self->mtx);

    if (self->reuse_count < self->reuse_capacity) {
        List_InsertBeforeFirst(&self->reuse_queue, &vp->owner_qe);
        self->reuse_count++;
        doReuse = true;
    }
    mtx_unlock(&self->mtx);
    

    // Suspend the VP if we decided to reuse it and schedule it for finalization
    // (termination) otherwise.
    if (doReuse) {
        vp->proc = NULL;
        vp->uerrno = 0;
        vp->psigs = 0;
        vp->sigmask = 0;
        vp->flags &= ~(VP_FLAG_USER_OWNED|VP_FLAG_ABORTED_USPACE);
        vp->lifecycle_state = VP_LIFECYCLE_RELINQUISHED;

        try_bang(vcpu_suspend(vp));
    }
    else {
        vcpu_terminate(vp);
    }
    /* NOT REACHED */
}
