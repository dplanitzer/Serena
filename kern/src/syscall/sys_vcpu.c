//
//  sys_vcpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kpi/vcpu.h>


SYSCALL_0(vcpu_errno)
{
    return (intptr_t)&(vp->uerrno);
}

SYSCALL_0(vcpu_getdata)
{
    return vp->udata;
}

SYSCALL_1(vcpu_setdata, intptr_t data)
{
    vp->udata = pa->data;
    return EOK;
}

SYSCALL_2(vcpu_acquire, const _vcpu_acquire_attr_t* _Nonnull attr, vcpuid_t* _Nonnull idp)
{
    vcpu_t new_vp;
    const errno_t err = Process_AcquireVirtualProcessor(vp->proc, pa->attr, &new_vp);

    if (err == EOK) {
        *(pa->idp) = new_vp->id;
    }
    return err;
}

SYSCALL_0(vcpu_relinquish_self)
{
    Process_RelinquishVirtualProcessor(vp->proc, vp);
    /* NOT REACHED */
    return 0;
}

static vcpu_t _Nullable _get_vcpu_by_id_locked(ProcessRef _Nonnull self, vcpuid_t id)
{
    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        if (cvp->id == id) {
            return cvp;
        }
    )

    return NULL;
}

SYSCALL_1(vcpu_suspend, vcpuid_t id)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->id == VCPUID_SELF || pa->id == vp->id) {
        // Suspending myself
        err = vcpu_suspend(vp);
    }
    else {
        // Suspending some other vcpu
        mtx_lock(&pp->mtx);
        vcpu_t vcp = _get_vcpu_by_id_locked(pp, pa->id);

        if (vcp) {
            err = vcpu_suspend(vcp);
        }
        else {
            err = ESRCH;
        }
        mtx_unlock(&pp->mtx);
    }

    return err;
}

SYSCALL_1(vcpu_resume, vcpuid_t id)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    vcpu_t vcp = _get_vcpu_by_id_locked(pp, pa->id);

    if (vcp) {
        vcpu_resume(vcp, false);
    }
    else {
        err = ESRCH;
    }
    mtx_unlock(&pp->mtx);

    return err;
}

SYSCALL_0(vcpu_yield)
{
    vcpu_yield();
    return EOK;
}

SYSCALL_3(vcpu_policy, vcpuid_t id, int version, vcpu_policy_t* _Nonnull policy)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->id == VCPUID_SELF || pa->id == vp->id) {
        err = vcpu_policy(vp, pa->version, pa->policy);
    }
    else {
        mtx_lock(&pp->mtx);
        vcpu_t vcp = _get_vcpu_by_id_locked(pp, pa->id);

        if (vcp) {
            err = vcpu_policy(vcp, pa->version, pa->policy);
        }
        else {
            err = ESRCH;
        }
        mtx_unlock(&pp->mtx);
    }

    return err;
}

SYSCALL_2(vcpu_setpolicy, vcpuid_t id, const vcpu_policy_t* _Nonnull policy)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->id == VCPUID_SELF || pa->id == vp->id) {
        err = vcpu_set_policy(vp, pa->policy);
    }
    else {
        mtx_lock(&pp->mtx);
        vcpu_t vcp = _get_vcpu_by_id_locked(pp, pa->id);

        if (vcp) {
            err = vcpu_set_policy(vcp, pa->policy);
        }
        else {
            err = ESRCH;
        }
        mtx_unlock(&pp->mtx);
    }

    return err;
}

SYSCALL_3(vcpu_state, vcpuid_t id, int flavor, vcpu_state_ref _Nonnull state)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->id == VCPUID_SELF || pa->id == vp->id) {
        err = vcpu_state(vp, pa->flavor, pa->state);
    }
    else {
        mtx_lock(&pp->mtx);
        vcpu_t vcp = _get_vcpu_by_id_locked(pp, pa->id);

        if (vcp) {
            err = vcpu_state(vcp, pa->flavor, pa->state);
        }
        else {
            err = ESRCH;
        }
        mtx_unlock(&pp->mtx);
    }

    return err;
}

SYSCALL_3(vcpu_setstate, vcpuid_t id, int flavor, const vcpu_state_ref _Nonnull state)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->id == VCPUID_SELF || pa->id == vp->id) {
        err = vcpu_set_state(vp, pa->flavor, pa->state);
    }
    else {
        mtx_lock(&pp->mtx);
        vcpu_t vcp = _get_vcpu_by_id_locked(pp, pa->id);

        if (vcp) {
            err = vcpu_set_state(vcp, pa->flavor, pa->state);
        }
        else {
            err = ESRCH;
        }
        mtx_unlock(&pp->mtx);
    }

    return err;
}

SYSCALL_3(vcpu_info, vcpuid_t id, int flavor, vcpu_info_ref _Nonnull info)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->id == VCPUID_SELF || pa->id == vp->id) {
        err = vcpu_info(vp, pa->flavor, pa->info);
    }
    else {
        mtx_lock(&pp->mtx);
        vcpu_t vcp = _get_vcpu_by_id_locked(pp, pa->id);

        if (vcp) {
            err = vcpu_info(vcp, pa->flavor, pa->info);
        }
        else {
            err = ESRCH;
        }
        mtx_unlock(&pp->mtx);
    }

    return err;
}
