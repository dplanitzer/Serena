//
//  fputs.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <limits.h>


int fputs(const char *str, FILE *s)
{
    ssize_t nCharsWritten = 0;
    ssize_t r;

    __fensure_no_err(s);
    __fensure_writeable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_Out);

    for (;;) {
        const char ch = *str;
 
        if (ch == '\0' || nCharsWritten == INT_MAX) {
            break;
        }
 
        r = __fputc(ch, s);
        if (r <= 0) {
            break;
        }

        nCharsWritten++;
        str++;
    }

    if (nCharsWritten > 0) {
        return (int)nCharsWritten;
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
