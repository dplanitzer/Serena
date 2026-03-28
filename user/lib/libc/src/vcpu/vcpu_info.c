//
//  vcpu_info.c
//  libc
//
//  Created by Dietmar Planitzer on 3/28/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <kpi/syscall.h>


int vcpu_info(vcpu_t _Nonnull vcpu, int flavor, vcpu_info_ref _Nonnull info)
{
    return (int)_syscall(SC_vcpu_info, (vcpu) ? vcpu->id : VCPUID_SELF, flavor, info);
}
