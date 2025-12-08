//
//  fseek.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"



int fseek(FILE *s, long offset, int whence)
{
    __fensure_seekable(s);

    switch (whence) {
        case SEEK_SET:
        case SEEK_CUR:
        case SEEK_END:
            break;

        default:
            errno = EINVAL;
            return EOF;
    }

    if (s->flags.direction == __kStreamDirection_Write) {
        if (fflush(s) != 0) {
            return EOF;
        }
    }

    const long long r = s->cb.seek((void*)s->context, (long long)offset, whence);
    if (r < 0ll) {
        s->flags.hasError = 1;
        return EOF;
    }
    if (!(offset == 0ll && whence == SEEK_CUR)) {
        s->flags.hasEof = 0;
    }
    // XXX drop ungetc buffered stuff

    return 0;
}
