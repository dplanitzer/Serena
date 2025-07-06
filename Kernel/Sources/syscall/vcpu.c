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

SYSCALL_0(vcpu_self)
{
    return (intptr_t)((VirtualProcessor*)p)->vpid;
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
