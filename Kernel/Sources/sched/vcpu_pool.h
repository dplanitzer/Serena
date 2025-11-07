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
#include <sched/mtx.h>


struct vcpu_pool {
    mtx_t   mtx;
    List    reuse_queue;        // VPs available for reuse
    int     reuse_count;        // count of how many VPs are in the reuse queue
    int     reuse_capacity;     // reuse cache will not store more than this. If a VP exits while the cache is at max capacity -> VP will exit for good and get finalized
};
typedef struct vcpu_pool* vcpu_pool_t;


extern vcpu_pool_t _Nonnull g_vcpu_pool;

extern errno_t vcpu_pool_create(vcpu_pool_t _Nullable * _Nonnull pOutSelf);
extern void vcpu_pool_destroy(vcpu_pool_t _Nullable self);

extern vcpu_t _Nullable vcpu_pool_checkout(vcpu_pool_t _Nonnull self);
extern bool vcpu_pool_checkin(vcpu_pool_t _Nonnull self, vcpu_t _Nonnull vp);

#endif /* _VCPU_POOL_H */
