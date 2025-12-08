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
// - 's' is writeable
// - 's' is byte-oriented
int __fputc(int ch, FILE * _Nonnull s)
{
    __fensure_no_err(s);

    unsigned char buf = (unsigned char)ch;
    const ssize_t nBytesWritten = s->cb.write((void*)s->context, &buf, 1);
    int r;

    if (nBytesWritten > 0) {
        s->flags.hasEof = 0;
        r = (int)buf;
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

int fputc(int ch, FILE *s)
{
    __fensure_no_err(s);
    __fensure_writeable(s);
    __fensure_byte_oriented(s);

    return __fputc(ch, s);
}
