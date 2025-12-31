//
//  atexit.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stdlib.h>


int atexit(void (*func)(void))
{
    int r = 0;

    mtx_lock(&__gAtExitLock);
    if (__gAtExitEnabled && __gAtExitFuncsCount < AT_EXIT_FUNCS_CAPACITY) {
        __gAtExitFuncs[__gAtExitFuncsCount++] = func;
    }
    else {
        r = -1;
    }
    mtx_unlock(&__gAtExitLock);

    return r;
}
