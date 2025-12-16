//
//  String.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <kern/string.h>


ssize_t strlen(const char* _Nonnull str)
{
    const char* p = str;

    while(*p++ != '\0');

    return p - str - 1;
}

ssize_t strnlen(const char* _Nonnull str, ssize_t strsz)
{
    ssize_t len = 0;

    while (*str++ != '\0' && len < strsz) {
        len++;
    }

    return len;
}

// Copies the characters of 'pSrc' to 'pDst'. Returns a pointer that points to
// the first byte past the '\0' byte in the destination string. 
char* _Nonnull strcpy(char* _Nonnull _Restrict dst, const char* _Nonnull _Restrict src)
{
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst++ = '\0';

    return dst;
}

char* _Nonnull strncpy(char* _Nonnull _Restrict dst, const char* _Nonnull _Restrict src, ssize_t count)
{
    while (*src != '\0' && count > 0) {
        *dst++ = *src++;
        count--;
    }
    if (count > 0) {
        *dst++ = '\0';
    }

    return dst;
}

bool strcmp(const char* _Nonnull lhs, const char* _Nonnull rhs)
{
    while (*lhs != '\0' && *lhs == *rhs) {
        lhs++;
        rhs++;
    }

    return (*lhs == *rhs) ? true : false;
}

bool strncmp(const char* _Nonnull lhs, const char* _Nonnull rhs, ssize_t count)
{
    while (count > 0 && *lhs != '\0' && *lhs == *rhs) {
        lhs++;
        rhs++;
        count--;
    }

    return (count == 0 || *lhs == *rhs) ? true : false;
}
