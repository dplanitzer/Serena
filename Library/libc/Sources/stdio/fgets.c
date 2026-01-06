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
    char* r = NULL;
    int nBytesToRead = count - 1;
    int nBytesRead = 0;
    ssize_t res;
    char ch;

    __flock(s);
    __fensure_no_eof_err(s);
    __fensure_readable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_In);

    if (count < 1) {
        errno = EINVAL;
        goto catch;
    }

    if (nBytesToRead > 0) {
        while (nBytesToRead-- > 0) {
            res = __fgetc(&ch, s);
            if (res <= 0) {
                break;
            }

            *str++ = ch;
            nBytesRead++;
            if (ch == '\n') {
                break;
            }
        }

        if (nBytesRead > 0) {
            *str = '\0';
            r = str;
        }
        else if (res == 0) {
            s->flags.hasEof = 1;
        }
        else {
            s->flags.hasError = 1;
        }
    }
    else {
        *str = '\0';
        r = str;
    }

catch:
    __funlock(s);
    return r;
}
