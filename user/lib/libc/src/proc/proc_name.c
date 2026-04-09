//
//  proc_name.c
//  libc
//
//  Created by Dietmar Planitzer on 3/28/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>


int proc_name(char* _Nonnull buf, size_t bufSize)
{
    return (int)_syscall(SC_proc_property, PROC_SELF, PROC_PROP_NAME, buf, bufSize);
}
