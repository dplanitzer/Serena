//
//  proc_acquire_vcpu.c
//  libc
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/proc.h>
#include <kpi/syscall.h>


int proc_acquire_vcpu(const vcpu_acquire_params_t* _Nonnull params, vcpuid_t* _Nonnull idp)
{
    return (int) _syscall(SC_acquire_vcpu, params, idp);
}
