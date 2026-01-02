//
//  vcpu_getschedparams.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <kpi/syscall.h>


int vcpu_getschedparams(vcpu_t _Nullable vcpu, int type, sched_params_t* _Nonnull params)
{
    return (int)_syscall(SC_vcpu_getschedparams, (vcpu) ? vcpu->id : VCPUID_SELF, type, params);
}

int vcpu_setschedparams(vcpu_t _Nullable vcpu, const sched_params_t* _Nonnull params)
{
    return (int)_syscall(SC_vcpu_setschedparams, (vcpu) ? vcpu->id : VCPUID_SELF, params);
}
