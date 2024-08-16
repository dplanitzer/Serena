//
//  strnlen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stddef.h>


size_t __strnlen(const char *str, size_t strsz)
{
    size_t len = 0;

    if (str) {
        while (*str++ != '\0' && len < strsz) {
            len++;
        }
    }

    return len;
}
