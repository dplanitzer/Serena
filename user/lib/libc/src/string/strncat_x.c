//
//  strncat_x.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 10/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//
#include <ext/string.h>


// Returns a pointer to the trailing '\0' of 'dst'. Even if 'src' is an empty string
char * _Nonnull strncat_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src, size_t max_chars)
{
    char *p = &dst[strlen(dst)];

    while (max_chars > 0 && *src != '\0') {
        *p++ = *src++;
        max_chars--;
    }
    *p = '\0';

    return p;
}
