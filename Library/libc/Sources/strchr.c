//
//  strchr.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stddef.h>


char *strchr(const char *str, int ch)
{
    const char c = (char)ch;

    while (*str != c && *str != '\0') {
        str++;
    }
    return (*str == c) ? (char *)str : NULL;
}
