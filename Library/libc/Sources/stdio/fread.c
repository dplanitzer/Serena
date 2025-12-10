//
//  fread.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <_math.h>


// Expects:
// - 's' is not NULL
// - 's' direction is in
// - 's' is readable
// - 's' is byte-oriented
static ssize_t __fread(FILE* _Nonnull _Restrict s, void * _Restrict buffer, ssize_t nbytes)
{
    if (nbytes == 0) {
        return 0;
    }

    return s->cb.read((void*)s->context, buffer, nbytes);
}

size_t fread(void * _Restrict buffer, size_t size, size_t count, FILE * _Restrict s)
{
    __fensure_no_eof_err(s);
    __fensure_readable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_In);

    if (size == 0 || count == 0) {
        return 0;
    }


    char* dst = buffer;
    uint64_t nBytesToRead = (uint64_t)size * (uint64_t)count;
    uint64_t nBytesRead = 0;
    ssize_t r;

    while (nBytesToRead > 0) {
        r = __fread(s, dst, (ssize_t)__min(nBytesToRead, (uint64_t)__SSIZE_MAX));
        if (r <= 0) {
            break;
        }

        nBytesToRead -= (uint64_t)r;
        nBytesRead += (uint64_t)r;
        dst += r;
    }


    if (nBytesRead > 0ull) {
        return nBytesRead / (uint64_t)size;
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
