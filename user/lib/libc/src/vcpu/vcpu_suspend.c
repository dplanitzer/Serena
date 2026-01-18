//
//  vcpu_suspend.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <kpi/syscall.h>


int vcpu_suspend(vcpu_t _Nullable vcpu)
{
    return (int)_syscall(SC_vcpu_suspend, (vcpu) ? vcpu->id : VCPUID_SELF);
}

void vcpu_resume(vcpu_t _Nonnull vcpu)
{
    (void)_syscall(SC_vcpu_resume, vcpu->id);
}
