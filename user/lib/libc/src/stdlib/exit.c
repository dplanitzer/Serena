//
//  exit.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <unistd.h>
#include <__stdlib.h>
#include <time.h>
#include <ext/timespec.h>


_Noreturn void exit(int status)
{
    // Disable the registration of any new atexit handlers.
    spin_lock(&__gAtExitLock);
    const bool isAlreadyExiting = __gIsExiting;
    __gIsExiting = true;
    spin_unlock(&__gAtExitLock);


    if (isAlreadyExiting) {
        // Some other vcpu has already started the exit() process. Wait until
        // the process is shot down by the kernel.
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &TIMESPEC_INF, NULL);
        /* NOT REACHED */
    }


    // It's safe now to access the atexit table without holding the lock
    while (__gAtExitFuncsCount-- > 0) {
        __gAtExitFuncs[__gAtExitFuncsCount]();
    }


    _exit(status);
}
