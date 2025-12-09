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
    s->bufferCount = 0;
}

// Flushes the buffered data in stream 's'.
// Expects:
// - 's' is not NULL
int __fflush(FILE * _Nonnull s)
{
    if (s->flags.direction != __kStreamDirection_Out) {
        // Ignore flush requests on non-output streams
        return 0;
    }

    const ssize_t nBytesWritten = s->cb.write((void*)s->context, s->buffer, s->bufferCount);
    int r;

    if (nBytesWritten > 0) {
        s->flags.hasEof = 0;
        r = 0;

        if (nBytesWritten < s->bufferCount) {
            // flush was partially successful
            s->bufferCount = s->bufferCount - nBytesWritten;
            memmove(s->buffer, &s->buffer[nBytesWritten], s->bufferCount);
        }
        else {
            s->bufferCount = 0;
        }
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

int fflush(FILE *s)
{
    if (s) {
        return __fflush(s);
    }
    else {
        return __iterate_open_files(__fflush);
    }
}
