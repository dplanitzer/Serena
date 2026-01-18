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
    int r = -1;

    spin_lock(&__gAtExitLock);
    if (!__gIsExiting && func && __gAtExitFuncsCount < ATEXIT_MAX) {
        __gAtExitFuncs[__gAtExitFuncsCount++] = func;
        r = 0;
    }
    spin_unlock(&__gAtExitLock);

    return r;
}
