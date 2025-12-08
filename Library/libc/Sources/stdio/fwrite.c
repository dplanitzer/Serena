//
//  fwrite.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


size_t fwrite(const void *buffer, size_t size, size_t count, FILE *s)
{
    if (size == 0 || count == 0) {
        return 0;
    }

    __fensure_no_err(s);
    __fensure_writeable(s);

    const size_t nBytesToWrite = size * count;
    const ssize_t nBytesWritten = s->cb.write((void*)s->context, buffer, nBytesToWrite);
    size_t r;

    if (nBytesWritten > 0) {
        s->flags.hasEof = 0;
        r = nBytesWritten / size;
    }
    else if (nBytesWritten == 0) {
        s->flags.hasEof = 1;
        r = EOF;
    }
    else {
        s->flags.hasError = 1;
        r = EOF;
    }

    return r;
}
