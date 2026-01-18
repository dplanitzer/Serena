//
//  memxxx_kei.c
//  libc
//
//  Created by Dietmar Planitzer on 12/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <__kei.h>
#include <stddef.h>

void * _Nonnull memcpy(void * _Nonnull _Restrict dst, const void * _Nonnull _Restrict src, size_t count)
{
    return ((void* (*)(void* _Restrict, const void* _Restrict, size_t))__gKeiTab[KEI_memcpy])(dst, src, count);
}

void * _Nonnull memmove(void * _Nonnull _Restrict dst, const void * _Nonnull _Restrict src, size_t count)
{
    return ((void* (*)(void* _Restrict, const void* _Restrict, size_t))__gKeiTab[KEI_memmove])(dst, src, count);
}

void * _Nonnull memset(void * _Nonnull dst, int c, size_t count)
{
    return ((void* (*)(void*, int, size_t))__gKeiTab[KEI_memset])(dst, c, count);
}
