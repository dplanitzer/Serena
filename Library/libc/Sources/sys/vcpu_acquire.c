//
//  vcpu_acquire.c
//  libc
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/vcpu.h>
#include <kpi/syscall.h>


int vcpu_acquire(const vcpu_acquire_params_t* _Nonnull params, vcpuid_t* _Nonnull idp)
{
    return (int) _syscall(SC_vcpu_acquire, params, idp);
}
