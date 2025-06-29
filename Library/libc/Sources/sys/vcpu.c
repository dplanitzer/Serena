//
//  vcpu.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/vcpu.h>
#include <kpi/syscall.h>


int vcpu_self(void)
{
    return (int)_syscall(SC_vcpu_self);
}

unsigned int vcpu_getsigmask(void)
{
    unsigned int mask;

    (void)_syscall(SC_vcpu_sigmask, SIGMASK_OP_ENABLE, 0, &mask);
    return mask;
}

int vcpu_setsigmask(int op, unsigned int mask, unsigned int* _Nullable oldmask)
{
    return (int)_syscall(SC_vcpu_sigmask, op, mask, oldmask);
}
