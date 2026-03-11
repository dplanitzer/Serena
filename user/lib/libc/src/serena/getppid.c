//
//  getppid.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>


pid_t getppid(void)
{
    return (pid_t)_syscall(SC_getppid);
}

pid_t getpgrp(void)
{
    return (pid_t)_syscall(SC_getpgrp);
}

pid_t getsid(void)
{
    return (pid_t)_syscall(SC_getsid);
}
