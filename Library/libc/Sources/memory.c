//
//  memory.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>


void *memchr(const void *ptr, int ch, size_t count)
{
    unsigned char *p = (unsigned char *)ptr;
    unsigned char c = (unsigned char)ch;

    while(count-- > 0) {
        if (*p++ == c) {
            return p;
        }
    }

    return NULL;
}

int memcmp(const void *lhs, const void *rhs, size_t count)
{
    unsigned char *plhs = (unsigned char *)lhs;
    unsigned char *prhs = (unsigned char *)rhs;
    
    while (count-- > 0) {
        if (*plhs++ != *prhs++) {
            return ((int)*plhs) - ((int)*prhs);
        }
    }

    return 0;
}

void *memset(void *dst, int ch, size_t count)
{
    unsigned char *p = (unsigned char *)dst;
    unsigned char c = (unsigned char)ch;

    while(count-- > 0) {
        *p++ = c;
    }

    return dst;
}

void *memcpy(void *dst, const void *src, size_t count)
{
    unsigned char *pdst = (unsigned char *)dst;
    const unsigned char *psrc = (const unsigned char *)src;

    while(count-- > 0) {
        *pdst++ = *psrc++;
    }

    return dst;
}

void *memmove(void *dst, const void *src, size_t count)
{
    // XXX handle overlaps
    unsigned char *pdst = (unsigned char *)dst;
    const unsigned char *psrc = (const unsigned char *)src;

    while(count-- > 0) {
        *pdst++ = *psrc++;
    }

    return dst;
}
