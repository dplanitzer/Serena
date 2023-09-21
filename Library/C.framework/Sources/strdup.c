//
//  strdup.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <stdlib.h>
#include <__stddef.h>


char *strdup(const char *src)
{
    const size_t len = strlen(src);
    char* dst = (char*) malloc(len);

    if (dst) {
        strcpy(dst, src);
    }
    return dst;
}

char *strndup(const char *src, size_t size)
{
    const size_t len = __strnlen(src, size);
    char* dst = (char*) malloc(len);

    if (dst) {
        strncpy(dst, src, len);
        dst[len] = '\0';
    }
    return dst;
}
