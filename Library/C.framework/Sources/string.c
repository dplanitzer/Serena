//
//  string.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>


size_t strlen(const char *str)
{
    size_t len = 0;

    while (*str++ != '\0') {
        len++;
    }

    return len;
}

char *strcpy(char *dst, const char *src)
{
    char *r = dst;

    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';

    return r;
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

char *strcat(char *dst, const char *src)
{
    if (*src != '\0') {
        char *p = &dst[strlen(dst)];

        while (*src != '\0') {
            *p++ = *src++;
        }
        *p = '\0';
    }

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
