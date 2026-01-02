//
//  __stdlib_init.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stdlib.h>
#include <stdio.h>
#include <string.h>
#include <kpi/kei.h>
#include <sys/_vcpu.h>

pargs_t*    __gProcessArguments;
kei_func_t* __gKeiTab;
char **     environ;

mtx_t                    __gAtExitLock;
at_exit_func_t _Nullable __gAtExitFuncs[AT_EXIT_FUNCS_CAPACITY];
int                      __gAtExitFuncsCount;
volatile bool            __gAtExitEnabled;


static void __exit_init(void)
{
    __gAtExitFuncsCount = 0;
    __gAtExitEnabled = true;
    mtx_init(&__gAtExitLock);
}

void __stdlibc_init(pargs_t* _Nonnull argsp)
{
    __gProcessArguments = argsp;
    __gKeiTab = argsp->urt_funcs;
    environ = argsp->envp;

    __vcpu_init();
    __malloc_init();
    __exit_init();
    __locale_init();
    __stdio_init();
}

// Returns true if the pointer is known as NOT free-able. Eg because it points
// to the text or read-only data segments or it points into the process argument
// area, etc.
bool __is_pointer_NOT_freeable(void* ptr)
{
    if (ptr >= (void*)__gProcessArguments && ptr < (void*)(((char*) __gProcessArguments) + __gProcessArguments->arguments_size)) {
        return true;
    }

    // XXX check text segment
    // XXX check read-only data segment

    return false;
}
