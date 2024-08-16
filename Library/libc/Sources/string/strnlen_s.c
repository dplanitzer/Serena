//
//  strnlen_s.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>


size_t strnlen_s(const char *str, size_t strsz)
{
    size_t len = 0;

    if (str) {
        while (*str++ != '\0' && len < strsz) {
            len++;
        }
    }

    return len;
}
