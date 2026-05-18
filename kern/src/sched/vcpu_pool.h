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

// Checks out a vcpu from teh vcpu pool. Returns NULL if no vcpu is available in
// the pool. The caller becomes the owner of the checked out vcpu. 
extern vcpu_t _Nullable vcpu_pool_checkout(vcpu_pool_t _Nonnull self);

// Checks the current (calling) vcpu into the vcpu pool. The vcpu is suspended
// before this call returns.
extern _Noreturn void vcpu_pool_checkin_current(vcpu_pool_t _Nonnull self);

extern void vcpu_pool_reaper_main(vcpu_pool_t _Nonnull self);

#endif /* _VCPU_POOL_H */
