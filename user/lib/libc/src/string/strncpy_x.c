//
//  strncpy_x.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 10/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/string.h>


char * _Nonnull strncpy_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src, size_t max_chars)
{
    while (max_chars > 0 && *src != '\0') {
        *dst++ = *src++;
        max_chars--;
    }
    *dst = '\0';

    return dst;
}
