//
//  strncpy.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stddef.h>


char * _Nonnull strncpy(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src, size_t count)
{
    char *r = dst;

    while (count > 0 && *src != '\0') {
        *dst++ = *src++;
        count--;
    }
    while (count-- > 0) {
        *dst++ = '\0';
    }

    return r;
}
