//
//  string.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <stdlib.h>
#include <__stddef.h>


size_t strlen(const char *str)
{
    size_t len = 0;

    while (*str++ != '\0') {
        len++;
    }

    return len;
}

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

// Similar to strcpy() but returns a pointer that points to the '\0 at the
// destination aka the end of the copied string. Exists so that we can actually
// use this and strcat() to compose strings without having to iterate over the
// same string multiple times.
char *__strcpy(char *dst, const char *src)
{
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';

    return dst;
}

char *strcpy(char *dst, const char *src)
{
    (void) __strcpy(dst, src);
    return dst;
}

char *strncpy(char *dst, const char *src, size_t count)
{
    char *r = dst;

    while (*src != '\0' && count > 0) {
        *dst++ = *src++;
        count--;
    }
    while (count-- > 0) {
        *dst++ = '\0';
    }

    return r;
}

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

char *strncat(char *dst, const char *src, size_t count)
{
    char *p = &dst[strlen(dst)];

    while (*src != '\0' && count > 0) {
        *p++ = *src++;
        count--;
    }
    *p = '\0';

    return dst;
}

int strcmp(const char *lhs, const char *rhs)
{
    while (*lhs != '\0') {
        const char d = *lhs++ - *rhs++;

        if (d != 0) {
            return d;
        }
    }

    return 0;
}

int strncmp(const char *lhs, const char *rhs, size_t count)
{
    while (*lhs != '\0' && count-- > 0) {
        const char d = *lhs++ - *rhs++;

        if (d != 0) {
            return d;
        }
    }

    return 0;
}

char *strchr(const char *str, int ch)
{
    const char c = (char)ch;

    while (*str != c && *str != '\0') {
        str++;
    }
    return (*str == c) ? (char *)str : NULL;
}

char *strrchr(const char *str, int ch)
{
    char *p = strchr(str, ch);

    if (ch == '\0' || p == NULL) {
        return p;
    }

    for (;;) {
        char *q = strchr(p + 1, ch);

        if (q == NULL) {
            break;
        }
        p = q;
    }

    return p;
}

char *strstr(const char *str, const char *sub_str)
{
    const char* s = str;
    const char* sub_s = sub_str;

    for (;;) {
        if (*sub_s == '\0') {
            return (char*) str;
        }
        if (*s == '\0') {
            return NULL;
        }

        if (*sub_s++ != *s++) {
            s = ++str;
            sub_s = sub_str;
        }
    }
}
