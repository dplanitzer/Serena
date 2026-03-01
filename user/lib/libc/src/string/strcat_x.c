//
//  strcat_x.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//
#if !defined(__M68K__)
#include <ext/string.h>


// See strcpy_x()
// Returns a pointer to the trailing '\0' of 'dst'. Even if 'src' is an empty string
char * _Nonnull strcat_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
{
    // 'dst' may point to a string - skip it
    while (*dst != '\0') {
        dst++;
    }

    // Append a copy of 'src' to the destination
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';

    return dst;
}
#endif
