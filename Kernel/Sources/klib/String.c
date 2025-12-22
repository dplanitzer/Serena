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

ssize_t strnlen(const char* _Nonnull str, ssize_t maxlen)
{
    ssize_t len = 0;

    while (len < maxlen && *str++ != '\0') {
        len++;
    }

    return len;
}

// Similar to strcpy() but returns a pointer that points to the '\0 at the
// destination aka the end of the copied string. Exists so that we can actually
// use this and strcat() to compose strings without having to iterate over the
// same string multiple times.
char * _Nonnull strcpy_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
{
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';

    return dst;
}

char * _Nonnull strcpy(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
{
    (void) strcpy_x(dst, src);
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
