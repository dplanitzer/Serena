//
//  fwrite.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <_math.h>
#include <string.h>


// Expects:
// - 's' is not NULL
// - 's' direction is out
// - 's' is writeable
// - 's' is byte-oriented
// - 'nbytes' > 0
// - buffer mode is _IOFBF or _IOLBF
static ssize_t __fwrite_bf(FILE * _Nonnull _Restrict s, const void * _Restrict buffer, ssize_t nbytes)
{
    const char* src = buffer;
    ssize_t nwritten = 0;
    ssize_t r;

    // Fill up the buffer as much as we can and then flush it
    if (s->bufferCount < s->bufferCapacity) {
        const ssize_t bufSize = s->bufferCapacity - s->bufferCount;
        const ssize_t nprefix = __min(bufSize, nbytes);

        memcpy(&s->buffer[s->bufferCount], src, nprefix);
        s->bufferCount += nprefix;
        nwritten += nprefix;
        nbytes -= nprefix;
        src += nprefix;
    }

    if (s->bufferCount == s->bufferCapacity && __fflush(s) == EOF) {
        return (nwritten > 0) ? nwritten : -1;
    }
    if (nbytes == 0) {
        return nwritten;
    }


    // We write the remainder directly to the stream if it wouldn't fit in the
    // buffer; otherwise we put the remainder into the buffer. We do a second
    // flush right away if we filled up the buffer again.
    if (nbytes > s->bufferCapacity) {
        r = s->cb.write((void*)s->context, src, nbytes);
        if (r >= 0) {
            return nwritten + r;
        }
        else {
            return (nwritten > 0) ? nwritten : -1;
        }
    }
    else {
        memcpy(s->buffer, src, nbytes);
        s->bufferCount += nbytes;

        // We ignore flush errors here because everything has fit into the buffer
        // and a future flush gets another chance to attempt to write to disk.
        if (s->bufferCount == s->bufferCapacity) {
            (void) __fflush(s);
        }
        return nwritten + nbytes;
    }
}

// - 's' is not NULL
// - 's' direction is out
// - 's' is writeable
// - 's' is byte-oriented
// - 'nbytes' > 0
// - buffer mode is _IOLBF
static ssize_t __fwrite_lbf(FILE * _Nonnull _Restrict s, const void * _Restrict buffer, ssize_t nbytes)
{
    const char* src = buffer;
    const char* psrc = src;
    ssize_t nwritten = 0;
    ssize_t r;

    for (;;) {
        const char ch = *src++;

        if (ch == '\0') {
            break;
        }
        if (ch == '\n') {
            r = __fwrite_bf(s, psrc, src - psrc);
            if (r < 0) {
                return (nwritten > 0) ? nwritten : -1;
            }

            nwritten += r;
            psrc = src;
        }
    }
    return nwritten;
}

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

    switch (s->flags.bufferMode) {
        case _IONBF:
            return s->cb.write((void*)s->context, buffer, nbytes);

        case _IOLBF:
            return __fwrite_lbf(s, buffer, nbytes);

        case _IOFBF:
            return __fwrite_bf(s, buffer, nbytes);
    }
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
