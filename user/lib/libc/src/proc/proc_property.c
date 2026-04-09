//
//  proc_property.c
//  libc
//
//  Created by Dietmar Planitzer on 4/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>


int proc_property(pid_t pid, int flavor, char* _Nonnull buf, size_t bufSize)
{
    return (int)_syscall(SC_proc_property, pid, flavor, buf, bufSize);
}
