//
//  host_info.c
//  libc
//
//  Created by Dietmar Planitzer on 4/2/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/host.h>
#include <kpi/syscall.h>


int host_info(int flavor, host_info_ref _Nonnull info)
{
    return (int)_syscall(SC_host_info, flavor, info);
}
