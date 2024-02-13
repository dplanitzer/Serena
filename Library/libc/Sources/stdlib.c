//
//  stdlib.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <__globals.h>
#include <__stddef.h>
#include <System/System.h>

void __stdlibc_init(ProcessArguments* _Nonnull argsp)
{
    __gProcessArguments = argsp;
    environ = argsp->envp;

    System_Init(argsp);
    __exit_init();
    __malloc_init();
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

int abs(int n)
{
    return __abs(n);
}

long labs(long n)
{
    return __abs(n);
}

long long llabs(long long n)
{
    return __abs(n);
}
