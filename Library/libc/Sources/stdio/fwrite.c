//
//  fwrite.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <_math.h>


// Expects:
// - 's' is not NULL
// - 's' direction is out
// - 's' is writeable
// - 's' is byte-oriented
ssize_t __fwrite(FILE * _Nonnull _Restrict s, const void * _Restrict buffer, ssize_t nbytes)
{
    if (nbytes == 0) {
        return 0;
    }

    __fensure_no_err(s);

    const ssize_t nBytesWritten = s->cb.write((void*)s->context, buffer, nbytes);
    ssize_t r;

    if (nBytesWritten > 0) {
        s->flags.hasEof = 0;
        r = nBytesWritten;
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

size_t fwrite(const void * _Restrict buffer, size_t size, size_t count, FILE * _Restrict s)
{
    __fensure_no_err(s);
    __fensure_writeable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_Out);

    if (size == 0 || count == 0) {
        return 0;
    }

    //XXX add support for buffering
    if (s->flags.bufferMode > _IONBF) {
        __fflush(s);
    }


    const char* src = buffer;
    uint64_t nBytesToWrite = (uint64_t)size * (uint64_t)count;
    uint64_t nBytesWritten = 0;

    while (nBytesToWrite > 0) {
        ssize_t nbytes = (ssize_t)__min(nBytesToWrite, (uint64_t)__SSIZE_MAX);
        const ssize_t r = __fwrite(s, src, nbytes);

        if (r == EOF) {
            return EOF;
        }

        nBytesToWrite -= (uint64_t)r;
        nBytesWritten += (uint64_t)r;
        src += r;
    }

    return nBytesWritten / (uint64_t)size;
}
