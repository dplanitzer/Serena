//
//  pipe.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/pipe.h>


int pipe(int fds[2])
{
    return (int)_syscall(SC_pipe, &fds[0], &fds[1]);
}
