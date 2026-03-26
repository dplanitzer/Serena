//
//  vcpu_policy.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <kpi/syscall.h>


int vcpu_policy(vcpu_t _Nullable vcpu, int version, vcpu_policy_t* _Nonnull policy)
{
    return (int)_syscall(SC_vcpu_policy, (vcpu) ? vcpu->id : VCPUID_SELF, version, policy);
}

int vcpu_setpolicy(vcpu_t _Nullable vcpu, const vcpu_policy_t* _Nonnull policy)
{
    return (int)_syscall(SC_vcpu_setpolicy, (vcpu) ? vcpu->id : VCPUID_SELF, policy);
}
