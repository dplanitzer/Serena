//
//  strncat.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>


char *strncat(char * _Restrict dst, const char * _Restrict src, size_t max_chars)
{
    char *p = &dst[strlen(dst)];

    while (max_chars > 0 && *src != '\0') {
        *p++ = *src++;
        max_chars--;
    }
    *p = '\0';

    return dst;
}
