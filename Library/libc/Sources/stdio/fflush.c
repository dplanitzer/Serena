//
//  Stream.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <string.h>


// Flushes the buffered data in stream 's'.
// Expects:
// - 's' is not NULL
// - 'dir' is different from the current stream direction
int __fsetdir(FILE * _Nonnull s, int dir)
{
    int r = 0;

    switch (s->flags.direction) {
        case __kStreamDirection_Unknown:
            // nothing to do
            break;

        case __kStreamDirection_In:
            __fdiscard(s);
            break;

        case __kStreamDirection_Out:
            r = __fflush(s);
            break;
    }

    s->flags.direction = dir;
    return r;
}

// Discards the buffered data in stream 's'.
// - 's' is not NULL
void __fdiscard(FILE * _Nonnull s)
{
    s->bufferIndex = 0;
    s->bufferCount = 0;
}

// Fills the buffer with data from stream 's'.
// Expects:
// - 's' is not NULL
// - 's' direction is in
// - buffer mode is _IOLBF or _IONBF (both modes are treated as _IOFBF for input purposes)
// - buffer is empty
// Returns 1 on success; 0 on EOF; -1 on error
int __ffill(FILE * _Nonnull s)
{
    assert(s->bufferIndex == s->bufferCount);

    const ssize_t r = s->cb.read(s->context, s->buffer, s->bufferCapacity);
    if (r <= 0) {
        return r;
    }

    s->bufferCount = r;
    s->bufferIndex = 0;

    return 1;
}

// Flushes the buffered data in stream 's'.
// Expects:
// - 's' is not NULL
// Returns EOF on error; 0 on success or when it didn't do anything
int __fflush(FILE * _Nonnull s)
{
    if (s->flags.direction != __kStreamDirection_Out || s->bufferCount == 0) {
        return 0;
    }

    const ssize_t r = s->cb.write(s->context, s->buffer, s->bufferCount);
    if (r < 0) {
        return EOF;
    }

    s->bufferCount -= r;
    if (s->bufferCount > 0) {
        // flush was partially successful
        memmove(s->buffer, &s->buffer[r], s->bufferCount);
    }
    return 0;
}

int fflush(FILE *s)
{
    if (s) {
        return __fflush(s);
    }
    else {
        return __iterate_open_files(__fflush);
    }
}
