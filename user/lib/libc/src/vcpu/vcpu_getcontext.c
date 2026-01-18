//
//  vcpu_getcontext.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <stdbool.h>
#include <kpi/syscall.h>


int vcpu_getcontext(vcpu_t _Nonnull vcpu, mcontext_t* _Nonnull ctx)
{
    //XXX if id == VCPUID_SELF -> read cpu state in user space
    return (int)_syscall(SC_vcpu_rw_mcontext, vcpu->id, ctx, true);
}

int vcpu_setcontext(vcpu_t _Nonnull vcpu, const mcontext_t* _Nonnull ctx)
{
    //XXX if id == VCPUID_SELF -> write cpu state in user space
    return (int)_syscall(SC_vcpu_rw_mcontext, vcpu->id, ctx, false);
}
