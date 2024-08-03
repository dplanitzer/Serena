//
//  strcmp.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stddef.h>


int strcmp(const char *lhs, const char *rhs)
{
    while (*lhs == *rhs) {
        if (*lhs == '\0') {
            return 0;
        }
        lhs++; rhs++;
    }

    return (*((unsigned char*)lhs) < *((unsigned char*)rhs)) ? -1 : 1;
}
