//
//  Stream.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <string.h>

static int _fflush(FILE* _Nonnull s);


// Flushes the buffered data in stream 's'.
// Expects:
// - 's' is not NULL
// - 'dir' is different from the current stream direction
int _fsetdir(FILE * _Nonnull s, int dir)
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
            r = _fflush(s);
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
// - 's' direction is out
// Returns the flush result code
flush_res_t __fflush(FILE * _Nonnull s)
{
    if (s->bufferCount == 0) {
        return kFlush_Ok;
    }

    const ssize_t nBytesWritten = s->cb.write((void*)s->context, s->buffer, s->bufferCount);
    if (nBytesWritten > 0) {
        if (nBytesWritten < s->bufferCount) {
            // flush was partially successful
            s->bufferCount = s->bufferCount - nBytesWritten;
            memmove(s->buffer, &s->buffer[nBytesWritten], s->bufferCount);
        }
        else {
            s->bufferCount = 0;
        }
        return kFlush_Ok;
    }
    else if (nBytesWritten == 0) {
        return kFlush_Eof;
    }
    else {
        return kFlush_Error;
    }
}

static int _fflush(FILE* _Nonnull s)
{
    if (s->flags.direction == __kStreamDirection_Out) {
        switch (__fflush(s)) {
            case kFlush_Ok:
                return 0;

            case kFlush_Eof:
                s->flags.hasEof = 1;
                return EOF;

            case kFlush_Error:
                s->flags.hasError = 1;
                return EOF;
        }
    }
    else {
        return 0;
    }
}

int fflush(FILE *s)
{
    if (s) {
        return _fflush(s);
    }
    else {
        return __iterate_open_files(_fflush);
    }
}
