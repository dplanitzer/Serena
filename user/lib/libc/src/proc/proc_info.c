//
//  proc_info.c
//  libc
//
//  Created by Dietmar Planitzer on 3/28/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>


int proc_info(pid_t _Nonnull pid, int flavor, proc_info_ref _Nonnull info)
{
    return (int)_syscall(SC_proc_info, pid, flavor, info);
}
