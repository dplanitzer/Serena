//
//  memory.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__globals.h>


void *memchr(const void *ptr, int ch, size_t count)
{
    unsigned char *p = (unsigned char *)ptr;
    unsigned char c = (unsigned char)ch;

    while(count-- > 0) {
        if (*p++ == c) {
            return p;
        }
    }

    return NULL;
}

int memcmp(const void *lhs, const void *rhs, size_t count)
{
    unsigned char *plhs = (unsigned char *)lhs;
    unsigned char *prhs = (unsigned char *)rhs;
    
    while (count-- > 0) {
        if (*plhs++ != *prhs++) {
            return ((int)*plhs) - ((int)*prhs);
        }
    }

    return 0;
}

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
