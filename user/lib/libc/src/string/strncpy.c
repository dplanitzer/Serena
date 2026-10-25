//
//  strncpy.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>


char * _Nonnull strncpy(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src, size_t max_chars)
{
    char *r = dst;

    while (max_chars > 0 && *src != '\0') {
        *dst++ = *src++;
        max_chars--;
    }
    while (max_chars-- > 0) {
        *dst++ = '\0';
    }

    return r;
}
