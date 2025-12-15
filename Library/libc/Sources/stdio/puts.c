//
//  puts.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <limits.h>
#include <string.h>


int puts(const char * _Nonnull str)
{
    __fensure_no_err(stdout, EOF);
    __fensure_writeable(stdout, EOF);
    __fensure_byte_oriented(stdout, EOF);
    __fensure_direction(stdout, __kStreamDirection_Out, EOF);

    const size_t len = __min(strlen(str), INT_MAX - 1);
    const ssize_t nCharsWritten = __fwrite(stdout, str, len);

    if (nCharsWritten >= 0) {
        if (__fputc('\n', stdout) == 1) {
            return (int)(nCharsWritten + 1);
        }
    }

    stdout->flags.hasError = 1;
    return EOF;
}
