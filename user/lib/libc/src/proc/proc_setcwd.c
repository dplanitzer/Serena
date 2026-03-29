//
//  proc_setcwd.c
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>


int proc_setcwd(const char* _Nonnull path)
{
    return (int)_syscall(SC_proc_setcwd, path);
}
