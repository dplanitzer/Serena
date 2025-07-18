//
//  getpid.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <kpi/syscall.h>


pid_t getpid(void)
{
    return (pid_t)_syscall(SC_getpid);
}
