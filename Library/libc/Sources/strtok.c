//
//  strtok.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <stdlib.h>
#include <__stddef.h>


static void __strmakset(const char* src, char ch_set[256])
{
    memset(ch_set, 0, 256);

    while (*src != '\0') {
        ch_set[*src++] = 1;
    }
}

static char *__str_first_not_in(const char *dst, char *ch_set)
{
    ch_set[0] = 0;
    while (ch_set[*dst]) {
        dst++;
    }


    // the '\0' marking the end of 'dst' is the first character not-in-set if
    // we end up scanning to the end of the string because the string ends in
    // characters from the 'ch_set' set.
    return (char*)dst;
}

static char *__str_first_in(const char *dst, char *ch_set)
{
    ch_set[0] = 1;
    while (!ch_set[*dst]) {
        dst++;
    }

    return (char*)dst;
}


size_t strspn(const char *dst, const char *src)
{
    // 'src' is really a set
    // We iterate 'dst' from the front until the check 'src contains dst[i]' fails.
    // Should change the bool array to a bit array one day. Don't feel like doing
    // this right now
    char keep_set[256];
    __strmakset(src, keep_set);

    return __str_first_not_in(dst, keep_set) - dst;
}

size_t strcspn(const char *dst, const char *src)
{
    char break_set[256];
    __strmakset(src, break_set);

    return __str_first_in(dst, break_set) - dst;
}

char *strpbrk(const char *dst, const char *break_set)
{
    char break_set_opt[256];
    __strmakset(break_set, break_set_opt);
    return __str_first_in(dst, break_set_opt);
}

char *strtok(char *str, const char *delim)
{
    static char* prev_tok_end_p;
    char delim_set[256];

    __strmakset(delim, delim_set);
    if (str == NULL) {
        str = prev_tok_end_p;
    }

    if (str) {
        // skip over the delimiter
        str = __str_first_not_in(str, delim_set);
    }

    if (*str == '\0') {
        return NULL;
    }

    // skip over the word until we hit a delimiter again
    char* word_start_p = str;
    str = __str_first_in(str, delim_set);
    if (*str != '\0') {
        *str = '\0';
        prev_tok_end_p = str + 1;
    } else {
        prev_tok_end_p = NULL;
    }

    return word_start_p;
}
