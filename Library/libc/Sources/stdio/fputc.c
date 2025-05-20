//
//  fputc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


int fputc(int ch, FILE *s)
{
    if (s->flags.hasError) {
        return EOF;
    }

    if ((s->flags.mode & __kStreamMode_Write) == 0) {
        s->flags.hasError = 1;
        errno = EBADF;
        return EOF;
    }

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
