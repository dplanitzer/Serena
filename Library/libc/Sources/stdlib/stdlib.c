//
//  stdlib.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <__globals.h>
#include <__stddef.h>
#include <kpi/kei.h>
#include <sys/proc.h>

extern void __kei_init(pargs_t* _Nonnull argsp);


void __stdlibc_init(pargs_t* _Nonnull argsp)
{
    __gProcessArguments = argsp;
    environ = argsp->envp;

    __kei_init(argsp);
    __malloc_init();
    __exit_init();
    __locale_init();
    __stdio_init();
}

// Returns true if the pointer is known as NOT freeable. Eg because it points
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
