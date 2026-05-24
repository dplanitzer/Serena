//
//  cpu_info.c
//  libc
//
//  Created by Dietmar Planitzer on 5/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/host.h>
#include <kpi/syscall.h>


int cpu_info(cpuid_t cpuid, int flavor, cpu_info_ref _Nonnull info)
{
    return (int)_syscall(SC_cpu_info, cpuid, flavor, info);
}
