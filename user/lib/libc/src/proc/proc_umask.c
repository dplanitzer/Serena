//
//  proc_umask.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>

mode_t proc_umask(mode_t mask)
{
    return _syscall(SC_proc_umask, mask);
}
