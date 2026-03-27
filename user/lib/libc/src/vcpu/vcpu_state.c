//
//  vcpu_state.c
//  libc
//
//  Created by Dietmar Planitzer on 3/25/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <kpi/syscall.h>


int vcpu_state(vcpu_t _Nonnull vcpu, int flavor, vcpu_state_ref _Nonnull state)
{
    return (int)_syscall(SC_vcpu_state, (vcpu) ? vcpu->id : VCPUID_SELF, flavor, state);
}

int vcpu_setstate(vcpu_t _Nonnull vcpu, int flavor, const vcpu_state_ref _Nonnull state)
{
    return (int)_syscall(SC_vcpu_setstate, (vcpu) ? vcpu->id : VCPUID_SELF, flavor, state);
}
