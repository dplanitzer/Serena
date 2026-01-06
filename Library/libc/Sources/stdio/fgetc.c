//
//  fgetc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


// Expects:
// - 's' is not NULL
// - 's' is readable
// - 's' direction is in
// - 's' is byte-oriented
// Returns 1 on success; 0 on EOF; -1 on error
ssize_t __fgetc(char* _Nonnull _Restrict pch, FILE * _Nonnull _Restrict s)
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

int fgetc(FILE * _Nonnull s)
{
    int r = EOF;

    __flock(s);
    __fensure_no_eof_err(s);
    __fensure_readable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_In);

    char ch;
    const ssize_t nBytesRead = __fgetc(&ch, s);

    if (nBytesRead == 1) {
        r = (int)ch;
    }
    else if (nBytesRead == 0) {
        s->flags.hasEof = 1;
    }
    else {
        s->flags.hasError = 1;
    }

catch:
    __funlock(s);
    return r;
}
