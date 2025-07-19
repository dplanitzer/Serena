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
#include <sched/mtx.h>
#include <sched/vcpu.h>


typedef struct VirtualProcessorParameters {
    VoidFunc_1 _Nonnull     func;
    void* _Nullable _Weak   context;
    VoidFunc_0 _Nullable    ret_func;
    size_t                  kernelStackSize;
    size_t                  userStackSize;
    vcpuid_t                vpgid;
    int                     priority;
    bool                    isUser;
} VirtualProcessorParameters;


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

extern errno_t vcpu_pool_acquire(vcpu_pool_t _Nonnull self, const VirtualProcessorParameters* _Nonnull params, vcpu_t _Nonnull * _Nonnull pOutVP);
extern _Noreturn vcpu_pool_relinquish(vcpu_pool_t _Nonnull self, vcpu_t _Nonnull vp);

#endif /* _VCPU_POOL_H */
