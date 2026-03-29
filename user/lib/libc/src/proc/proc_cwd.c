//
//  proc_cwd.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>


int proc_cwd(char* _Nonnull buffer, size_t bufferSize)
{
    return (int)_syscall(SC_proc_cwd, buffer, bufferSize);
}
