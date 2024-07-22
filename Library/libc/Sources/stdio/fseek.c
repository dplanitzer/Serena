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
    if (s->cb.seek == NULL) {
        errno = ESPIPE;
        return EOF;
    }
    switch (whence) {
        case SEEK_SET:
        case SEEK_CUR:
        case SEEK_END:
            break;

        default:
            errno = EINVAL;
            return EOF;
    }

    if (s->flags.mostRecentDirection == __kStreamDirection_Write) {
        const int e = fflush(s);
        if (e != 0) {
            return EOF;
        }
    }

    const errno_t err = s->cb.seek((void*)s->context, (long long)offset, NULL, whence);
    if (err != 0) {
        s->flags.hasError = 1;
        errno = err;
        return EOF;
    }
    if (!(offset == 0ll && whence == SEEK_CUR)) {
        s->flags.hasEof = 0;
    }
    // XXX drop ungetc buffered stuff

    return 0;
}
