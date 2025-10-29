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
        mtx_deinit(&self->mtx);
        kfree(self);
    }
}

errno_t vcpu_pool_acquire(vcpu_pool_t _Nonnull self, const vcpu_activation_t* _Nonnull act, vcpu_t _Nonnull * _Nonnull pOutVP)
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
        try(vcpu_create(&act->schedParams, &vp));
    }
    
    
    err = vcpu_activate(vp, act);

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

    // Try to cache the VP
    mtx_lock(&self->mtx);
    if (self->reuse_count < self->reuse_capacity) {
        List_InsertBeforeFirst(&self->reuse_queue, &vp->owner_qe);
        self->reuse_count++;
        doReuse = true;
    }
    mtx_unlock(&self->mtx);
    

    vcpu_deactivate(vp);

    if (doReuse) {
        try_bang(vcpu_suspend(vp));
    }
    else {
        vcpu_terminate(vp);
    }
    /* NOT REACHED */
}
