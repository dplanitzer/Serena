//
//  clock_wait.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <sys/_syscall.h>


int clock_wait(clockid_t clock, const struct timespec* _Nonnull delay)
{
    return (int)_syscall(SC_clock_wait, clock, delay);
}
