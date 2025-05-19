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
    if (s->flags.hasEof || s->flags.hasError) {
        return EOF;
    }
    
    if ((s->flags.mode & __kStreamMode_Read) == 0) {
        s->flags.hasError = 1;
        errno = EBADF;
        return EOF;
    }

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
