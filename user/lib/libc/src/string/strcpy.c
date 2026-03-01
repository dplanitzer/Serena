//
//  strcpy.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//
#if !defined(__M68K__)
#include <string.h>


char * _Nonnull strcpy(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
{
    char * r = dst;

    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';

    return r;
}
#endif
