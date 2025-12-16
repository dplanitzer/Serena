//
//  strdup.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <stdlib.h>
#include <__stddef.h>


char * _Nullable strdup(const char * _Nonnull src)
{
    const size_t lenWithNul = strlen(src) + 1;
    char* dst = malloc(lenWithNul);

    if (dst) {
        memcpy(dst, src, lenWithNul);
    }
    return dst;
}
