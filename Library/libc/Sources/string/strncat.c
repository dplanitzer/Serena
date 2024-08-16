//
//  strncat.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stddef.h>


char *strncat(char * _Restrict dst, const char * _Restrict src, size_t count)
{
    char *p = &dst[strlen(dst)];

    while (count > 0 && *src != '\0') {
        *p++ = *src++;
        count--;
    }
    *p = '\0';

    return dst;
}
