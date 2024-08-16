//
//  strndup.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
#include <stdlib.h>
#include <__stddef.h>


char *strndup(const char *src, size_t size)
{
    const size_t len = strnlen_s(src, size);
    char* dst = malloc(len + 1);

    if (dst) {
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return dst;
}
