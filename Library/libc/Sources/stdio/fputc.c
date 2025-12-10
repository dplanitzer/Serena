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
// Returns the number of bytes written on success; < 0 on error
ssize_t __fputc(char ch, FILE * _Nonnull s)
{
    char bufMode = s->flags.bufferMode;


    if (bufMode == _IONBF) {
        return s->cb.write((void*)s->context, &ch, 1);
    }

    if (ch == '\n' && bufMode == _IOLBF) {
        // Put the newline into the buffer if possible so that we can flush
        // the line together with its terminating \n in one go. That way a
        // line-based reader can pick up the whole line quicker.
        const int doOpt = (s->bufferCount < s->bufferCapacity);

        if (doOpt) s->buffer[s->bufferCount++] = '\n';

        if (__fflush(s) == EOF) {
            return -1;
        }

        if (!doOpt) s->buffer[s->bufferCount++] = '\n';
        return 1;
    }
    else if (s->bufferCount == s->bufferCapacity) {
        if (__fflush(s) == EOF) {
            return -1;
        }
    }

    s->buffer[s->bufferCount++] = ch;
    return 1;
}

int fputc(int ch, FILE *s)
{
    const char ch8 = (const unsigned char)ch;

    __fensure_no_err(s);
    __fensure_writeable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_Out);

    const ssize_t r = __fputc(ch8, s);

    if (r == 1) {
        return (int)ch8;
    }
    else {
        s->flags.hasError = 1;
        return EOF;
    }
}
