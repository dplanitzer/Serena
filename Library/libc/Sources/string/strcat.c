//
//  strcat.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <ext/string.h>


// See __strcpy()
char * _Nonnull strcat_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
{
    char* p = dst;

    if (*src != '\0') {
        // 'dst' may point to a string - skip it
        while (*p != '\0') {
            p++;
        }

        // Append a copy of 'src' to the destination
        while (*src != '\0') {
            *p++ = *src++;
        }
        *p = '\0';
    }

    return p;
}

char * _Nonnull strcat(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
{
    (void) strcat_x(dst, src);
    return dst;
}
