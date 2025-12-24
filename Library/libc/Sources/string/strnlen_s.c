//
//  strnlen_s.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>


size_t strnlen_s(const char * _Nullable str, size_t maxlen)
{
    size_t len = 0;

    if (str) {
        while (*str++ != '\0' && len < maxlen) {
            len++;
        }
    }

    return len;
}
