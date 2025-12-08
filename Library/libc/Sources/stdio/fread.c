//
//  fread.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


size_t fread(void *buffer, size_t size, size_t count, FILE *s)
{
    if (size == 0 || count == 0) {
        return 0;
    }

    __fensure_no_eof_err(s);
    __fensure_readable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_In);

    const size_t nBytesToRead = size * count;
    ssize_t nBytesRead = s->cb.read((void*)s->context, buffer, nBytesToRead);
    size_t r;

    if (nBytesRead > 0) {
        s->flags.hasEof = 0;
        r = nBytesRead / size;
    }
    else if (nBytesRead == 0) {
        s->flags.hasEof = 1;
        r = EOF;
    }
    else {
        s->flags.hasError = 1;
        r = EOF;
    }

    return r;
}
