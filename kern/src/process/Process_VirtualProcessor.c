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
    VoidFunc_0 ret_func = NULL;
    vcpu_acquisition_t ac;

    if (_proc_is_terminating(self)) {
        throw(ECANCELED);
    }


    ac.func = (VoidFunc_1)attr->func;
    ac.arg = attr->arg;
    ac.ret_func = (is_uproc) ? uproc_relinquish_vcpu_self : kproc_relinquish_vcpu_self;
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


    vcpu_relinquish_current();
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
