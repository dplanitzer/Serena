//
//  fgets.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


// Expects:
// 'count' > 0
static int _fgets(char* _Nonnull _Restrict str, int count, FILE* _Nonnull _Restrict s)
{
    int nBytesToRead = count - 1;
    int nBytesRead = 0;
    ssize_t r;
    char ch;

    if (count < 1) {
        errno = EINVAL;
        return EOF;
    }

    __fensure_no_eof_err(s);
    __fensure_readable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_In);


    while (nBytesToRead-- > 0) {
        r = __fgetc(&ch, s);
        if (r <= 0) {
            break;
        }

        *str++ = ch;
        nBytesRead++;
        if (ch == '\n') {
            break;
        }
    }
    *str = '\0';


    if (nBytesRead > 0 || nBytesToRead == 0) {
        return (int)nBytesRead;
    }
    else if (r == 0) {
        s->flags.hasEof = 1;
        return EOF;
    }
    else {
        s->flags.hasError = 1;
        return EOF;
    }
}

char * _Nonnull fgets(char * _Nonnull _Restrict str, int count, FILE * _Nonnull _Restrict s)
{
    return (_fgets(str, count, s) != EOF) ? str : NULL;
}
