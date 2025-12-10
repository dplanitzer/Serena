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

    //XXX add support for buffering
    if (s->flags.bufferMode > _IONBF) {
        if (__fflush(s) == EOF) {
            return -1;
        }
    }

    return s->cb.write((void*)s->context, buffer, nbytes);
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


    const char* src = buffer;
    uint64_t nBytesToWrite = (uint64_t)size * (uint64_t)count;
    uint64_t nBytesWritten = 0;
    ssize_t r;

    while (nBytesToWrite > 0) {
        r = __fwrite(s, src, (ssize_t)__min(nBytesToWrite, (uint64_t)__SSIZE_MAX));
        if (r <= 0) {
            break;
        }

        nBytesToWrite -= (uint64_t)r;
        nBytesWritten += (uint64_t)r;
        src += r;
    }

    if (nBytesWritten >= 0ull) {
        return nBytesWritten / (uint64_t)size;
    }
    else {
        s->flags.hasError = 1;
        return EOF;
    }
}
