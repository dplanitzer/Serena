//
//  strcat.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stddef.h>


// See __strcpy()
char *__strcat(char *dst, const char *src)
{
    char* p = dst;

    if (*src != '\0') {
        // 'dst' may point to a string - skip it
        while (*p != '\0') {
            p++;
        }

        // Append a copy of 'src' to the destination
        while (*src != '\0') {
            *p++ = *src++;
        }
        *p = '\0';
    }

    return p;
}

char *strcat(char *dst, const char *src)
{
    (void) __strcat(dst, src);
    return dst;
}
