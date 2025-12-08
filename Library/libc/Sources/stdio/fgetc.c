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
// - 's' is byte-oriented
int __fgetc(FILE * _Nonnull s)
{
    __fensure_no_eof_err(s);

    char buf;
    ssize_t nBytesRead = s->cb.read((void*)s->context, &buf, 1);
    int r;

    if (nBytesRead > 0) {
        s->flags.hasEof = 0;
        r = (int)(unsigned char)buf;
    }
    else if (nBytesRead == 0) {
        s->flags.hasEof = 1;
        r = EOF;
    }
    else {
        s->flags.hasError = 1;
        r = EOF;
    }

    return r;
}

int fgetc(FILE *s)
{
    __fensure_no_eof_err(s);
    __fensure_readable(s);
    __fensure_byte_oriented(s);

    return __fgetc(s);
}
