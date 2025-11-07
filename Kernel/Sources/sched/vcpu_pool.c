//
//  vcpu_pool.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "vcpu_pool.h"
#include <kern/kalloc.h>
#include <sched/vcpu.h>


vcpu_pool_t g_vcpu_pool;


errno_t vcpu_pool_create(vcpu_pool_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    vcpu_pool_t self;
    
    try(kalloc_cleared(sizeof(struct vcpu_pool), (void**) &self));
    mtx_init(&self->mtx);
    self->reuse_capacity = 32;
    
catch:
    *pOutSelf = self;
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
    vcpu_t vp;

    mtx_lock(&self->mtx);
    if (!List_IsEmpty(&self->reuse_queue)) {
        vp = vcpu_from_owner_qe(List_RemoveFirst(&self->reuse_queue));
        self->reuse_count--;
    }
    else {
        vp = NULL;
    }    
    mtx_unlock(&self->mtx);

    return vp;
}

bool vcpu_pool_checkin(vcpu_pool_t _Nonnull self, vcpu_t _Nonnull vp)
{
    bool reused;

    mtx_lock(&self->mtx);
    if (self->reuse_count < self->reuse_capacity) {
        List_InsertAfterLast(&self->reuse_queue, &vp->owner_qe);
        self->reuse_count++;
        reused = true;
    }
    else {
        reused = false;
    }
    mtx_unlock(&self->mtx);

    return reused;
}
