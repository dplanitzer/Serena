//
//  fread.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


size_t fread(void *buffer, size_t size, size_t count, FILE *s)
{
    if (size == 0 || count == 0) {
        return 0;
    }

    if (s->flags.hasEof || s->flags.hasError) {
        return EOF;
    }
    
    if ((s->flags.mode & __kStreamMode_Read) == 0) {
        s->flags.hasError = 1;
        errno = EBADF;
        return EOF;
    }

    const size_t nBytesToRead = size * count;
    size_t r;
    ssize_t nBytesRead;
    const errno_t err = s->cb.read((void*)s->context, buffer, nBytesToRead, &nBytesRead);

    if (err == 0) {
        if (nBytesRead > 0) {
            s->flags.hasEof = 0;
            r = nBytesRead / size;
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
