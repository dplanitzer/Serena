//
//  fwrite.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


size_t fwrite(const void *buffer, size_t size, size_t count, FILE *s)
{
    if (size == 0 || count == 0) {
        return 0;
    }

    if (s->flags.hasError) {
        return EOF;
    }

    if ((s->flags.mode & __kStreamMode_Write) == 0) {
        s->flags.hasError = 1;
        errno = EBADF;
        return EOF;
    }

    const size_t nBytesToWrite = size * count;
    ssize_t nBytesWritten;
    size_t r;
    const errno_t err = s->cb.write((void*)s->context, buffer, nBytesToWrite, &nBytesWritten);

    if (err == 0) {
        if (nBytesWritten > 0) {
            s->flags.hasEof = 0;
            r = nBytesWritten / size;
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
