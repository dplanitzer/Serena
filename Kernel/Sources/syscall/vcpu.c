//
//  vcpu.c
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
    return (intptr_t)vp->vpid;
}

SYSCALL_0(vcpu_getgrp)
{
    return (intptr_t)vp->vpgid;
}

SYSCALL_3(vcpu_setsigmask, int op, sigset_t mask, sigset_t* _Nullable oldmask)
{
    return VirtualProcessor_SetSignalMask(vp, pa->op, pa->mask & ~SIGSET_NONMASKABLES, pa->oldmask);
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

// Trap #2
void _vcpu_relinquish_self(void)
{
    VirtualProcessor* vp = VirtualProcessor_GetCurrent();

    Process_RelinquishVirtualProcessor(vp->proc, vp);
    /* NOT REACHED */
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
        err = VirtualProcessor_Suspend(vp);
    }
    else {
        Lock_Lock(&pp->lock);
        List_ForEach(&pp->vpQueue, ListNode, {
            VirtualProcessor* cvp = VP_FROM_OWNER_NODE(pCurNode);

            if (cvp->vpid == pa->id) {
                err = VirtualProcessor_Suspend(cvp);
                break;
            }
        });
        Lock_Unlock(&pp->lock);
    }

    return err;
}

SYSCALL_1(vcpu_resume, vcpuid_t id)
{
    ProcessRef pp = vp->proc;

    Lock_Lock(&pp->lock);
    List_ForEach(&pp->vpQueue, ListNode, {
        VirtualProcessor* cvp = VP_FROM_OWNER_NODE(pCurNode);

        if (cvp->vpid == pa->id) {
            VirtualProcessor_Resume(cvp, false);
            break;
        }
    });
    Lock_Unlock(&pp->lock);

    return EOK;
}

SYSCALL_0(vcpu_yield)
{
    VirtualProcessor_Yield();
    return EOK;
}
