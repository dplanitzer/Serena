//
//  vcpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"


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

SYSCALL_0(vcpu_relinquish_self)
{
    VirtualProcessor* pp = (VirtualProcessor*)p;

    Process_RelinquishVirtualProcessor(pp->proc, pp);
    /* NOT REACHED */
    return 0;
}
