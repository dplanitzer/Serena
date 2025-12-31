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


_Noreturn exit(int status)
{
    // Disable the registration of any new atexit handlers.
    mtx_lock(&__gAtExitLock);
    __gAtExitEnabled = false;
    mtx_unlock(&__gAtExitLock);


    // It's safe now to access the atexit table without holding the lock
    while (__gAtExitFuncsCount-- > 0) {
        __gAtExitFuncs[__gAtExitFuncsCount]();
    }


    _exit(status);
}
