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
#include <sched/vcpu.h>


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

vcpu_t _Nullable vcpu_pool_checkout(vcpu_pool_t _Nonnull self)
{
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

    return vp;
}

bool vcpu_pool_checkin(vcpu_pool_t _Nonnull self, vcpu_t _Nonnull vp)
{
    bool reused = false;

    // Try to cache the VP
    mtx_lock(&self->mtx);
    if (self->reuse_count < self->reuse_capacity) {
        List_InsertBeforeFirst(&self->reuse_queue, &vp->owner_qe);
        self->reuse_count++;
        reused = true;
    }
    mtx_unlock(&self->mtx);

    return reused;
}
