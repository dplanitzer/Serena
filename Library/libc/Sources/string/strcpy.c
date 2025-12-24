//
//  strcpy.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>


// Similar to strcpy() but returns a pointer that points to the '\0 at the
// destination aka the end of the copied string. Exists so that we can actually
// use this and strcat() to compose strings without having to iterate over the
// same string multiple times.
char * _Nonnull strcpy_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
{
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';

    return dst;
}

char * _Nonnull strcpy(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
{
    (void) strcpy_x(dst, src);
    return dst;
}
