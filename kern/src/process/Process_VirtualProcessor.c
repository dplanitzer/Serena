//
//  Process_VirtualProcessor.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <assert.h>
#include <string.h>
#include <kern/kalloc.h>
#include <kpi/syscall.h>
#include <sched/vcpu_pool.h>


_Noreturn void uproc_relinquish_vcpu_self(void)
{
    _syscall(SC_vcpu_relinquish_self);
    /* NOT REACHED */
}

static _Noreturn void kproc_relinquish_vcpu_self(void)
{
    Process_RelinquishCurrentVirtualProcessor(vcpu_current()->proc);
    /* NOT REACHED */
}

errno_t _proc_acquire_vcpu(ProcessRef _Nonnull _Locked self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpu_t _Nullable * _Nonnull pOutVp)
{
    decl_try_err();
    const bool is_uproc = _proc_is_user(self);
    vcpu_t vp = NULL;
    bool doFree = false;

    // Validate 'attr'
    try(validate_vcpu_policy(&attr->policy));
    if (attr->func == NULL) {
        throw(EINVAL);
    }


    // Don't acquire a new vcpu if we're in teh process of terminating
    if (_proc_is_terminating(self)) {
        throw(ECANCELED);
    }


    // Try to get a vcpu from the global pool
    vp = vcpu_pool_checkout(g_vcpu_pool);


    // Create a new vcpu if we were not able to reuse a cached one
    if (vp == NULL) {
        try(kalloc_cleared(sizeof(struct vcpu), (void**) &vp));

        vcpu_init(vp);
        doFree = true;
    }


    // Note that a vcpu freshly checked out from the pool may not have entered
    // the suspend state yet. Wait until it is actually suspended and before we
    // proceed with reconfiguring it. We only become the owner of the vcpu once
    // it has entered suspended state.
    while (vcpu_await_suspension(vp) != EOK);

    
    //
    // The vcpu is guaranteed to be suspended at this point
    //

    // First wipe out the old state and create a clean slate. We do this here
    // because doing this at relinquish time would be unsafe since the vcpu is
    // still running at the start of the relinquish phase
    vcpu_reset(vp, &attr->policy, self->sched_nice, self->quantum_boost);


    // Setup kernel and user stacks
    const size_t kernelStackSize = min_vcpu_kernel_stack_size();
    const size_t userStackSize = (is_uproc) ? __max(attr->stack_size, PROC_DEFAULT_USER_STACK_SIZE) : 0;

    try(stk_setmaxsize(&vp->kernel_stack, kernelStackSize));
    try(stk_setmaxsize(&vp->user_stack, userStackSize));

    VoidFunc_0 ret_func = (is_uproc) ? uproc_relinquish_vcpu_self : kproc_relinquish_vcpu_self;
    _vcpu_reset_stacks(vp, attr->func, attr->arg, ret_func, is_uproc, true);

    
    // Setup tag, id, group id, etc
    vp->tag = (is_uproc) ? VP_TAG_USER : VP_TAG_SYS;
    vp->id = self->next_avail_vcpuid++;
    vp->group_id = attr->group_id;
    vp->proc = self;
    vp->udata = attr->data;

    deque_add_last(&self->vcpu_queue, &vp->owner_qe);
    self->vcpu_count++;
    self->vcpu_lifetime_count++;

    clock_gettime(g_mono_clock, &vp->acquisition_time);


catch:
    if (err != EOK && doFree) {
        vcpu_destroy(vp);
    }

    *pOutVp = vp;
    return err;
}

errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpu_t _Nullable * _Nonnull pOutVp)
{
    decl_try_err();
    vcpu_t vp = NULL;

    mtx_lock(&self->mtx);
    err = _proc_acquire_vcpu(self, attr, &vp);
    mtx_unlock(&self->mtx);

    if (err == EOK) {
        *pOutVp = vp;

        if ((attr->flags & VCPU_ACQUIRE_RESUMED) == VCPU_ACQUIRE_RESUMED) {
            vcpu_resume(vp, false);
        }
    }

    return err;
}

_Noreturn void Process_RelinquishCurrentVirtualProcessor(ProcessRef _Nonnull self)
{
    vcpu_t vp = vcpu_current();

    assert(vp->proc == self);

    mtx_lock(&self->mtx);
    deque_remove(&self->vcpu_queue, &vp->owner_qe);

    self->rq_user_ticks += vp->user_ticks;
    self->rq_system_ticks += vp->system_ticks;
    self->rq_wait_ticks += vp->wait_ticks;
    self->vcpu_count--;

    mtx_unlock(&self->mtx);


    vcpu_pool_checkin(g_vcpu_pool, vp);
    try_bang(vcpu_suspend(vp));
    /* NOT REACHED */
}

bool proc_is_last_vcpu(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const bool r = (self->vcpu_count == 1) ? true : false;
    mtx_unlock(&self->mtx);

    return r;
}

vcpu_t _Nullable _proc_vcpu_for_id(ProcessRef _Nonnull self, vcpuid_t id)
{
    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        if (cvp->id == id) {
            return cvp;
        }
    )

    return NULL;
}

errno_t Process_GetVirtualProcessorIds(ProcessRef _Nonnull self, vcpuid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore)
{
    decl_try_err();
    size_t idx = 0;

    // Min is 2 because there must be one vcpu that is executing this code + the trailing 0
    if (bufSize < 2) {
        return ERANGE;
    }

    mtx_lock(&self->mtx);
    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        buf[idx++] = cvp->id;
        if (idx == bufSize-1) {
            break;
        }
    )

    buf[idx] = 0;
    *out_hasMore = (self->vcpu_count > idx) ? 1 : 0;

    mtx_unlock(&self->mtx);

    return err;
}
