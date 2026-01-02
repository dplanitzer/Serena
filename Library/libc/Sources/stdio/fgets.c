//
//  fgets.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


char * _Nonnull fgets(char * _Nonnull _Restrict str, int count, FILE * _Nonnull _Restrict s)
{
    int nBytesToRead = count - 1;
    int nBytesRead = 0;
    ssize_t r;
    char ch;

    __fensure_no_eof_err(s, NULL);
    __fensure_readable(s, NULL);
    __fensure_byte_oriented(s, NULL);
    __fensure_direction(s, __kStreamDirection_In, NULL);

    if (count < 1) {
        errno = EINVAL;
        return NULL;
    }


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
        return str;
    }
    else if (r == 0) {
        s->flags.hasEof = 1;
        return NULL;
    }
    else {
        s->flags.hasError = 1;
        return NULL;
    }
}
