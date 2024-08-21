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

    int r;
    unsigned char buf = (unsigned char)ch;
    ssize_t nBytesWritten;
    const errno_t err = s->cb.write((void*)s->context, &buf, 1, &nBytesWritten);

    if (err == 0) {
        if (nBytesWritten == 1) {
            s->flags.hasEof = 0;
            r = (int)buf;
        } else {
            s->flags.hasEof = 1;
            r = EOF;
        }
    } else {
        s->flags.hasError = 1;
        errno = err;
        r = EOF;
    }

    return r;
}

int putchar(int ch)
{
    return putc(ch, stdout);
}
