//
//  pipe.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <sys/_syscall.h>


int pipe(int fds[2])
{
    return (int)_syscall(SC_pipe, &fds[0], &fds[1]);
}
