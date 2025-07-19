//
//  exit.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mtx.h>
#include <sys/proc.h>
#include <kpi/syscall.h>
#include <__stddef.h>


#define AT_EXIT_FUNCS_CAPACITY   32
typedef void (*at_exit_func_t)(void);


static mtx_t                  __gAtExitLock;
static at_exit_func_t _Nullable __gAtExitFuncs[AT_EXIT_FUNCS_CAPACITY];
static int                      __gAtExitFuncsCount;
static volatile bool            __gAtExitEnabled;


void __exit_init(void)
{
    // XXX protect with a lock
    __gAtExitFuncsCount = 0;
    __gAtExitEnabled = true;
    mtx_init(&__gAtExitLock);
}

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
