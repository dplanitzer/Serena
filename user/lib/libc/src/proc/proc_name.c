//
//  proc_name.c
//  libc
//
//  Created by Dietmar Planitzer on 3/28/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>


int proc_name(pid_t pid, char* _Nonnull buf, size_t bufSize)
{
    return (int)_syscall(SC_proc_name, pid, buf, bufSize);
}
