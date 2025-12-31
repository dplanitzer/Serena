//
//  fread.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <string.h>
#include <ext/limits.h>
#include <ext/math.h>


// Expects:
// - 's' is not NULL
// - 's' direction is in
// - 's' is readable
// - 's' is byte-oriented
static ssize_t __fread(FILE* _Nonnull _Restrict s, void * _Nonnull _Restrict buffer, ssize_t nbytes)
{
    char* dst = buffer;
    ssize_t r, nread = 0;

    if (nbytes == 0) {
        return 0;
    }

    if (s->flags.bufferMode == _IONBF) {
        if (s->ugbCount > 0) {
            if (__fget_ugb(&dst[0], s) == EOF) {
                return EOF;
            }

            dst++;
            nbytes--;
            if (nbytes == 0) {
                return 1;
            }
        }

        return s->cb.read(s->context, dst, nbytes);
    }

    // _IOLBF or _IOFBF
    // Read what we can from the buffer
    if (s->bufferIndex < s->bufferCount) {
        const ssize_t bufSize = s->bufferCount - s->bufferIndex;
        const ssize_t nprefix = __min(bufSize, nbytes);

        memcpy(dst, &s->buffer[s->bufferIndex], nprefix);
        s->bufferIndex += nprefix;
        nread += nprefix;
        nbytes -= nprefix;
        dst += nprefix;
    }
    
    if (nbytes == 0) {
        return nread;
    }


    // We read the remainder directly from the file if the remainder is bigger
    // than our buffer capacity; otherwise we re-fill our buffer and read the
    // remainder from the buffer
    if (nbytes >= s->bufferCapacity) {
        r = s->cb.read(s->context, dst, nbytes);
        if (r <= 0) {
            return (nread > 0) ? nread : r;
        }
        else {
            return nread + r;
        }
    }
    else {
        // Buffer is empty at this point. Re-fill it
        r = __ffill(s);
        if (r <= 0) {
            return (nread > 0) ? nread : r;
        }

        memcpy(dst, s->buffer, nbytes);
        s->bufferIndex += nbytes;

        return nread + nbytes;
    }
}

size_t fread(void * _Nonnull _Restrict buffer, size_t size, size_t count, FILE * _Nonnull _Restrict s)
{
    __fensure_no_eof_err(s, 0);
    __fensure_readable(s, 0);
    __fensure_byte_oriented(s, 0);
    __fensure_direction(s, __kStreamDirection_In, 0);

    if (size == 0 || count == 0) {
        return 0;
    }


    char* dst = buffer;
    uint64_t nBytesToRead = (uint64_t)size * count;
    uint64_t nBytesRead = 0;
    ssize_t r;

    while (nBytesToRead > 0) {
        r = __fread(s, dst, (ssize_t)__min(nBytesToRead, (uint64_t)SSIZE_MAX));
        if (r <= 0) {
            break;
        }

        nBytesToRead -= (uint64_t)r;
        nBytesRead += (uint64_t)r;
        dst += r;
    }


    if (nBytesRead > 0ull) {
        return nBytesRead / size;
    }
    else if (r == 0) {
        s->flags.hasEof = 1;
        return 0;
    }
    else {
        s->flags.hasError = 1;
        return 0;
    }
}
