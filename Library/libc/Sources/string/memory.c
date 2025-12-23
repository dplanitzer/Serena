//
//  memory.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <sys/proc.h>

extern pargs_t* __gProcessArguments;


void * _Nonnull memset(void * _Nonnull dst, int c, size_t count)
{
    return ((void* (*)(void*, int, size_t))__gProcessArguments->urt_funcs[KEI_memset])(dst, c, count);
}

void * _Nonnull memcpy(void * _Nonnull _Restrict dst, const void * _Nonnull _Restrict src, size_t count)
{
    return ((void* (*)(void* _Restrict, const void* _Restrict, size_t))__gProcessArguments->urt_funcs[KEI_memcpy])(dst, src, count);
}

void * _Nonnull memmove(void * _Nonnull _Restrict dst, const void * _Nonnull _Restrict src, size_t count)
{
    return ((void* (*)(void* _Restrict, const void* _Restrict, size_t))__gProcessArguments->urt_funcs[KEI_memmove])(dst, src, count);
}
