//
//  fgetc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


// Expects:
// - 's' is not NULL
// - 's' is readable
// - 's' direction is in
// - 's' is byte-oriented
// Returns 1 on success; 0 on EOF; -1 on error
ssize_t __fgetc(char* _Nonnull pch, FILE * _Nonnull s)
{
    if (s->flags.bufferMode == _IONBF) {
        if (s->ugbCount == 0) {
            return s->cb.read(s->context, pch, 1);
        }
        else {
            return __fget_ugb(pch, s);
        }
    }

    // _IONBF or _IOLBF
    if (s->bufferIndex == s->bufferCount) {
        const ssize_t r = __ffill(s);

        if (r <= 0) {
            return r;
        }
    }

    *pch = s->buffer[s->bufferIndex++];
    return 1;
}

int fgetc(FILE *s)
{
    __fensure_no_eof_err(s, EOF);
    __fensure_readable(s, EOF);
    __fensure_byte_oriented(s, EOF);
    __fensure_direction(s, __kStreamDirection_In, EOF);

    char ch;
    const ssize_t r = __fgetc(&ch, s);

    if (r == 1) {
        return (int)ch;
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
