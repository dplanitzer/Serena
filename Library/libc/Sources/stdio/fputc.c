//
//  fputc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


// Expects:
// - 's' is not NULL
// - 's' direction is out
// - 's' is writeable
// - 's' is byte-oriented
int __fputc(int ch, FILE * _Nonnull s)
{
    char bufMode = s->flags.bufferMode;
    unsigned char ch8 = (unsigned char)ch;
    int r;

    __fensure_no_err(s);


    if (bufMode > _IONBF) {
        if (s->bufferCount == s->bufferCapacity) {
            if (__fflush(s) == EOF) {
                return EOF;
            }
        }
        else if (ch8 == '\n' && bufMode == _IOLBF) {
            // Put the newline into the buffer if possible so that we can flush
            // the line together with its terminating \n in one go. That way a
            // line-based reader can pick up the whole line quicker.
            const int doOpt = (s->bufferCount < s->bufferCapacity);

            if (doOpt) s->buffer[s->bufferCount++] = '\n';
            s->flags.hasEof = 0;

            if (__fflush(s) == EOF) {
                return EOF;
            }

            if (!doOpt) s->buffer[s->bufferCount++] = '\n';
            return (int)ch8;
        }
        
        s->buffer[s->bufferCount++] = ch8;
        s->flags.hasEof = 0;
        r = (int)ch8;
    }
    else {
        const ssize_t nBytesWritten = s->cb.write((void*)s->context, &ch8, 1);

        if (nBytesWritten > 0) {
            s->flags.hasEof = 0;
            r = (int)ch8;
        }
        else if (nBytesWritten == 0) {
            s->flags.hasEof = 1;
            r = EOF;
        }
        else {
            s->flags.hasError = 1;
            r = EOF;
        }
    }

    return r;
}

int fputc(int ch, FILE *s)
{
    __fensure_no_err(s);
    __fensure_writeable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_Out);

    return __fputc(ch, s);
}
