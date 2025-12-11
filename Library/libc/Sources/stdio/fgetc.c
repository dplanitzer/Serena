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
ssize_t __fgetc(char* _Nonnull pch, FILE * _Nonnull s)
{
    return s->cb.read((void*)s->context, pch, 1);
}

int fgetc(FILE *s)
{
    __fensure_no_eof_err(s);
    __fensure_readable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_In);

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
