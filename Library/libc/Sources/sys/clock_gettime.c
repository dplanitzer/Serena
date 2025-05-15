//
//  clock_gettime.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <System/_syscall.h>
#include <System/_varargs.h>


int clock_gettime(clockid_t clock, struct timespec* _Nonnull ts)
{
    return (int)_syscall(SC_clock_gettime, clock, ts);
}
