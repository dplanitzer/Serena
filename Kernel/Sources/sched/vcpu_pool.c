//
//  vcpu_pool.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "vcpu_pool.h"
#include "sched.h"
#include <kern/kalloc.h>


vcpu_pool_t g_vcpu_pool;


errno_t vcpu_pool_create(vcpu_pool_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    vcpu_pool_t self;
    
    try(kalloc_cleared(sizeof(struct vcpu_pool), (void**) &self));
    List_Init(&self->reuse_queue);
    mtx_init(&self->mtx);
    self->reuse_capacity = 16;
    *pOutSelf = self;
    return EOK;
    
catch:
    vcpu_pool_destroy(self);
    *pOutSelf = NULL;
    return err;
}

void vcpu_pool_destroy(vcpu_pool_t _Nullable self)
{
    if (self) {
        List_Deinit(&self->reuse_queue);
        mtx_deinit(&self->mtx);
        kfree(self);
    }
}

errno_t vcpu_pool_acquire(vcpu_pool_t _Nonnull self, const VirtualProcessorParameters* _Nonnull params, vcpu_t _Nonnull * _Nonnull pOutVP)
{
    decl_try_err();
    vcpu_t vp = NULL;

    mtx_lock(&self->mtx);

    // Try to reuse a cached VP
    List_ForEach(&self->reuse_queue, ListNode, {
        vcpu_t cvp = vcpu_from_owner_qe(pCurNode);

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
    vp->id = params->id;
    vp->groupid = params->groupid;
    vp->lifecycle_state = VP_LIFECYCLE_ACQUIRED;

catch:
    *pOutVP = vp;
    return err;
}

// Relinquishes the given VP back to the reuse pool if possible. If the reuse
// pool is full then the given VP is suspended and scheduled for finalization
// instead. Note that the VP is suspended in any case.
_Noreturn vcpu_pool_relinquish(vcpu_pool_t _Nonnull self, vcpu_t _Nonnull vp)
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
        vp->udata = 0;
        vp->id = 0;
        vp->groupid = 0;
        vp->uerrno = 0;
        vp->pending_sigs = 0;
        vp->proc_sigs_enabled = 0;
        vp->flags &= ~(VP_FLAG_USER_OWNED|VP_FLAG_HANDLING_EXCPT);
        vp->lifecycle_state = VP_LIFECYCLE_RELINQUISHED;

        try_bang(vcpu_suspend(vp));
    }
    else {
        vcpu_terminate(vp);
    }
    /* NOT REACHED */
}
