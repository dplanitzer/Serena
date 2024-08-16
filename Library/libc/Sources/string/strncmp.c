//
//  strncmp.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stddef.h>


int strncmp(const char *lhs, const char *rhs, size_t count)
{
    while (count > 0 && *lhs == *rhs) {
        if (*lhs == '\0') {
            return 0;
        }
        lhs++; rhs++; count--;
    }

    return (count == 0) ? 0 : (*((unsigned char*)lhs) < *((unsigned char*)rhs)) ? -1 : 1;
}
