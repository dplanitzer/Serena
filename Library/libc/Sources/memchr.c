//
//  memchr.c
//  libc
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
