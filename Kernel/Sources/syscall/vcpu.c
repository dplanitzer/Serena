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
    return (intptr_t)&(((VirtualProcessor*)p)->uerrno);
}

SYSCALL_0(vcpu_getid)
{
    return (intptr_t)((VirtualProcessor*)p)->vpid;
}

SYSCALL_0(vcpu_getgrp)
{
    return (intptr_t)((VirtualProcessor*)p)->vpgid;
}

SYSCALL_3(vcpu_setsigmask, int op, sigset_t mask, sigset_t* _Nullable oldmask)
{
    return VirtualProcessor_SetSignalMask((VirtualProcessor*)p, pa->op, pa->mask & ~SIGSET_NONMASKABLES, pa->oldmask);
}

SYSCALL_0(vcpu_getdata)
{
    return ((VirtualProcessor*)p)->udata;
}

SYSCALL_1(vcpu_setdata, intptr_t data)
{
    ((VirtualProcessor*)p)->udata = pa->data;
    return EOK;
}

SYSCALL_2(vcpu_acquire, const _vcpu_acquire_attr_t* _Nonnull attr, vcpuid_t* _Nonnull idp)
{
    return Process_AcquireVirtualProcessor((ProcessRef)p, pa->attr, pa->idp);
}

SYSCALL_0(vcpu_relinquish_self)
{
    VirtualProcessor* pp = (VirtualProcessor*)p;

    Process_RelinquishVirtualProcessor(pp->proc, pp);
    /* NOT REACHED */
    return 0;
}

SYSCALL_1(vcpu_suspend, vcpuid_t id)
{
    decl_try_err();
    VirtualProcessor* vp = (VirtualProcessor*)p;
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
    ProcessRef pp = (ProcessRef)p;

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
