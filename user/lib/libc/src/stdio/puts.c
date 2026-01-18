//
//  puts.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <limits.h>
#include <string.h>


int puts(const char * _Nonnull str)
{
    int r = EOF;

    __flock(stdout);
    __fensure_no_err(stdout);
    __fensure_writeable(stdout);
    __fensure_byte_oriented(stdout);
    __fensure_direction(stdout, __kStreamDirection_Out);

    const size_t len = __min(strlen(str), INT_MAX - 1);
    const ssize_t nCharsWritten = __fwrite(stdout, str, len);

    if (nCharsWritten >= 0 && __fputc('\n', stdout) == 1) {
        r = (int)(nCharsWritten + 1);
    }
    else {
        stdout->flags.hasError = 1;
    }

catch:
    __funlock(stdout);
    return r;
}
