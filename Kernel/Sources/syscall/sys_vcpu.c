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

SYSCALL_0(vcpu_getid)
{
    return (intptr_t)vp->id;
}

SYSCALL_0(vcpu_getgrp)
{
    return (intptr_t)vp->groupid;
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
    List_ForEach(&self->vcpu_queue, ListNode,
        vcpu_t cvp = vcpu_from_owner_qe(pCurNode);

        if (cvp->id == id) {
            return cvp;
        }
    );

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

SYSCALL_3(vcpu_rw_mcontext, vcpuid_t id, mcontext_t* _Nonnull ctx, int isRead)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    vcpu_t vcp = _get_vcpu_by_id_locked(pp, pa->id);

    if (vcp) {
        vcpu_rw_mcontext(vcp, pa->ctx, pa->isRead);
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

SYSCALL_3(vcpu_getschedparams, vcpuid_t id, int type, sched_params_t* _Nonnull params)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->id == VCPUID_SELF) {
        err = vcpu_getschedparams(vp, pa->type, pa->params);
    }
    else {
        mtx_lock(&pp->mtx);
        vcpu_t vcp = _get_vcpu_by_id_locked(pp, pa->id);

        if (vcp) {
            err = vcpu_getschedparams(vcp, pa->type, pa->params);
        }
        else {
            err = ESRCH;
        }
        mtx_unlock(&pp->mtx);
    }

    return err;
}

SYSCALL_2(vcpu_setschedparams, vcpuid_t id, const sched_params_t* _Nonnull params)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->id == VCPUID_SELF) {
        err = vcpu_setschedparams(vp, pa->params);
    }
    else {
        mtx_lock(&pp->mtx);
        vcpu_t vcp = _get_vcpu_by_id_locked(pp, pa->id);

        if (vcp) {
            err = vcpu_setschedparams(vcp, pa->params);
        }
        else {
            err = ESRCH;
        }
        mtx_unlock(&pp->mtx);
    }

    return err;
}
