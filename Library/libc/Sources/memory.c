//
//  memory.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__globals.h>


void *memset(void *dst, int c, size_t count)
{
    return ((void* (*)(void*, int, size_t))__gProcessArguments->urt_funcs[kUrtFunc_memset])(dst, c, count);
}

void *memcpy(void * _Restrict dst, const void * _Restrict src, size_t count)
{
    return ((void* (*)(void* _Restrict, const void* _Restrict, size_t))__gProcessArguments->urt_funcs[kUrtFunc_memcpy])(dst, src, count);
}

void *memmove(void * _Restrict dst, const void * _Restrict src, size_t count)
{
    return ((void* (*)(void* _Restrict, const void* _Restrict, size_t))__gProcessArguments->urt_funcs[kUrtFunc_memmove])(dst, src, count);
}
