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

#define LOW_WATER_MARK      30  /* how many vcpus to keep in the pool */
#define REAPING_THRESHOLD   33  /* when the reaper should be kicked off */
#define HIGH_WATER_MARK     36  /* when reaping should be expedited */

vcpu_pool_t g_vcpu_pool;


errno_t vcpu_pool_create(vcpu_pool_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    vcpu_pool_t self;
    
    try(kalloc_cleared(sizeof(struct vcpu_pool), (void**) &self));
    mtx_init(&self->mtx);
    cnd_init(&self->cnd);
    
    self->reaper_bkg_policy.version = sizeof(vcpu_policy_t);
    self->reaper_bkg_policy.qos.grade = VCPU_QOS_BACKGROUND;
    self->reaper_bkg_policy.qos.priority = VCPU_PRI_LOWEST + 1;

    self->reaper_urg_policy.version = sizeof(vcpu_policy_t);
    self->reaper_urg_policy.qos.grade = VCPU_QOS_URGENT;
    self->reaper_urg_policy.qos.priority = VCPU_PRI_HIGHEST;

catch:
    *pOutSelf = self;
    return err;
}

vcpu_t _Nullable vcpu_pool_checkout(vcpu_pool_t _Nonnull self)
{
    vcpu_t vp;

    mtx_lock(&self->mtx);
    if (!deque_empty(&self->q)) {
        vp = vcpu_from_owner_qe(deque_remove_first(&self->q));
        self->count--;
    }
    else {
        vp = NULL;
    }
    mtx_unlock(&self->mtx);

    return vp;
}

void vcpu_pool_checkin(vcpu_pool_t _Nonnull self, vcpu_t _Nonnull vp)
{
    mtx_lock(&self->mtx);

    deque_add_last(&self->q, &vp->owner_qe);
    self->count++;

    if (self->count > REAPING_THRESHOLD) {
        if (self->count > HIGH_WATER_MARK) {
            vcpu_set_policy(self->reaper_vcpu, &self->reaper_urg_policy);
        }
        cnd_signal(&self->cnd);
    }
    mtx_unlock(&self->mtx);
}

void vcpu_pool_reaper_main(vcpu_pool_t _Nonnull self)
{
    mtx_lock(&self->mtx);
    self->reaper_vcpu = vcpu_current();

    for (;;) {
        deque_for_each(&self->q, deque_node_t, it,
            if (self->count <= LOW_WATER_MARK) {
                break;
            }

            vcpu_t vp = vcpu_from_owner_qe(it);

            if (vp->run_state == VCPU_STATE_SUSPENDED) {
                deque_remove(&self->q, it);
                self->count--;

                vcpu_destroy(vp);
            }
        );


        // The reaper runs at Background QoS by default and is only pulled up
        // to Urgent QoS if too many vcpus have accumulated in the pool
        vcpu_set_policy(self->reaper_vcpu, &self->reaper_bkg_policy);


        // Sleep until we got something to do
        cnd_wait(&self->cnd, &self->mtx);
    }

    mtx_unlock(&self->mtx);
}
