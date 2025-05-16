//
//  getppid.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <sys/_syscall.h>
#include <kern/_varargs.h>


pid_t getppid(void)
{
    return (pid_t)_syscall(SC_getppid);
}
