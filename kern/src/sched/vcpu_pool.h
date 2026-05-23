//
//  vcpu_pool.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _VCPU_POOL_H
#define _VCPU_POOL_H 1

#include <stdbool.h>
#include <kpi/vcpu.h>
#include <sched/cnd.h>
#include <sched/mtx.h>


struct vcpu_pool {
    mtx_t           mtx;
    cnd_t           cnd;
    
    deque_t         q;      // pool queue
    int             count;  // count of how many VPs are in the pool

    vcpu_t _Nonnull reaper_vcpu;
    vcpu_policy_t   reaper_bkg_policy;
    vcpu_policy_t   reaper_urg_policy;
};
typedef struct vcpu_pool* vcpu_pool_t;


extern vcpu_pool_t _Nonnull g_vcpu_pool;

extern errno_t vcpu_pool_create(vcpu_pool_t _Nullable * _Nonnull pOutSelf);

// Checks out a vcpu from the vcpu pool. Returns NULL if there's not enough
// memory to create/check-out a vcpu. The checked out vcpu is in SUSPENDED state
// and the caller becomes the owner of this vcpu. 
extern vcpu_t _Nullable vcpu_pool_checkout(vcpu_pool_t _Nonnull self);

// Checks the vcpu 'vp' into the vcpu pool and suspends it. Note that this
// function does not return if 'vp' is the currently running vcpu.
extern void vcpu_pool_checkin(vcpu_pool_t _Nonnull self, vcpu_t _Nonnull vp);

// Returns the number of vcpus in the pool.
extern size_t vcpu_pool_size(vcpu_pool_t _Nonnull self);

extern void vcpu_pool_reaper_main(vcpu_pool_t _Nonnull self);

#endif /* _VCPU_POOL_H */
