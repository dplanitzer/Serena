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
    int nCharsWritten = 0;

    __fensure_no_err(s);
    __fensure_writeable(s);
    __fensure_byte_oriented(s);

    while (*str != '\0') {
        const int r = __fputc(*str++, s);

        if (r == EOF) {
            return EOF;
        }

        if (nCharsWritten < INT_MAX) {
            nCharsWritten++;
        }
    }
    return nCharsWritten;
}
