//
//  _exit.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <sys/_syscall.h>


_Noreturn _exit(int status)
{
    (void)_syscall(SC_exit, status);
}
