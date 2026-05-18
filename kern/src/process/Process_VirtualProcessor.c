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


static _Noreturn void _vcpu_relinquish_self(void)
{
    Process_RelinquishCurrentVirtualProcessor(vcpu_current()->proc);
    /* NOT REACHED */
}

errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpu_t _Nullable * _Nonnull pOutVp)
{
    decl_try_err();
    const bool is_uproc = _proc_is_user(self);
    vcpu_t vp = NULL;
    vcpu_acquisition_t ac;

    mtx_lock(&self->mtx);
    if (_proc_is_terminating(self)) {
        throw(ECANCELED);
    }

    ac.func = (VoidFunc_1)attr->func;
    ac.arg = attr->arg;
    ac.ret_func = (is_uproc) ? vcpu_uret_relinquish_self : _vcpu_relinquish_self;
    ac.kernelStackBase = NULL;
    ac.kernelStackSize = 0;
    ac.userStackSize = (is_uproc) ? __max(attr->stack_size, PROC_DEFAULT_USER_STACK_SIZE) : 0;
    ac.id = self->next_avail_vcpuid++;
    ac.group_id = attr->group_id;
    ac.policy = attr->policy;
    ac.sched_nice = self->sched_nice;
    ac.sched_quantum_boost = self->quantum_boost;
    ac.isUser = is_uproc;

    try(vcpu_acquire(&ac, &vp));
    
    vp->proc = self;
    vp->udata = attr->data;
    deque_add_last(&self->vcpu_queue, &vp->owner_qe);
    self->vcpu_count++;
    self->vcpu_lifetime_count++;
    
catch:
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


    vcpu_relinquish(vcpu_current());
    /* NOT REACHED */
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
