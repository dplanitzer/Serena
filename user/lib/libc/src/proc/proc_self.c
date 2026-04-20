//
//  proc_self.c
//  libc
//
//  Created by Dietmar Planitzer on 3/29/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>


pid_t proc_self(void)
{
    return (pid_t)_syscall(SC_proc_self);
}
