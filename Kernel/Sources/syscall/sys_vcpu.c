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
    return Process_AcquireVirtualProcessor(vp->proc, pa->attr, pa->idp);
}

SYSCALL_0(vcpu_relinquish_self)
{
    Process_RelinquishVirtualProcessor(vp->proc, vp);
    /* NOT REACHED */
    return 0;
}

SYSCALL_1(vcpu_suspend, vcpuid_t id)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->id == VCPUID_SELF) {
        err = vcpu_suspend(vp);
    }
    else {
        mtx_lock(&pp->mtx);
        List_ForEach(&pp->vcpu_queue, ListNode,
            vcpu_t cvp = VP_FROM_OWNER_NODE(pCurNode);

            if (cvp->id == pa->id) {
                err = vcpu_suspend(cvp);
                break;
            }
        );
        mtx_unlock(&pp->mtx);
    }

    return err;
}

SYSCALL_1(vcpu_resume, vcpuid_t id)
{
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    List_ForEach(&pp->vcpu_queue, ListNode,
        vcpu_t cvp = VP_FROM_OWNER_NODE(pCurNode);

        if (cvp->id == pa->id) {
            vcpu_resume(cvp, false);
            break;
        }
    );
    mtx_unlock(&pp->mtx);

    return EOK;
}

SYSCALL_0(vcpu_yield)
{
    vcpu_yield();
    return EOK;
}
