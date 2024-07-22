//
//  fgetc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


int fgetc(FILE *s)
{
    if ((s->flags.mode & __kStreamMode_Read) == 0) {
        s->flags.hasError = 1;
        errno = EBADF;
        return EOF;
    }

    int r;
    char buf;
    ssize_t nBytesRead;
    const errno_t err = s->cb.read((void*)s->context, &buf, 1, &nBytesRead);

    if (err == 0) {
        if (nBytesRead == 1) {
            s->flags.hasEof = 0;
            r = (int)(unsigned char)buf;
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

int getchar(void)
{
    return getc(stdin);
}
