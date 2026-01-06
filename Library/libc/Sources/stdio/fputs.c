//
//  fputs.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <limits.h>
#include <string.h>


int fputs(const char * _Nonnull _Restrict str, FILE * _Nonnull _Restrict s)
{
    int r = EOF;

    __flock(s);
    __fensure_no_err(s);
    __fensure_writeable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_Out);

    const size_t len = __min(strlen(str), INT_MAX);
    const ssize_t nCharsWritten = __fwrite(s, str, len);

    if (nCharsWritten >= 0) {
        r = (int)nCharsWritten;
    }
    else {
        s->flags.hasError = 1;
    }

catch:
    __funlock(s);
    return r;
}
