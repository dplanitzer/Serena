//
//  fseek.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


int fseeko(FILE * _Nonnull s, off_t offset, int whence)
{
    switch (whence) {
        case SEEK_SET:
        case SEEK_CUR:
        case SEEK_END:
            break;

        default:
            errno = EINVAL;
            return EOF;
    }

    __fensure_seekable(s, EOF);
    __fensure_direction(s, __kStreamDirection_Unknown, EOF);

    const long long r = s->cb.seek((void*)s->context, (long long)offset, whence);
    if (r < 0ll) {
        s->flags.hasError = 1;
        return EOF;
    }
    __fdiscard_ugb(s);
    s->flags.hasEof = 0;

    return 0;
}

int fseek(FILE * _Nonnull s, long offset, int whence)
{
    return fseeko(s, (off_t)offset, whence);
}
