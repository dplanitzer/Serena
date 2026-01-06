//
//  fseeko.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


int __fseeko(FILE * _Nonnull s, off_t offset, int whence)
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

    int r = EOF;

    __fensure_seekable(s);
    __fensure_direction(s, __kStreamDirection_Unknown);

    const long long res = s->cb.seek((void*)s->context, (long long)offset, whence);
    if (res >= 0ll) {
        __fdiscard_ugb(s);
        s->flags.hasEof = 0;
        r = 0;
    }
    else {
        s->flags.hasError = 1;
    }

catch:
    return r;
}

int fseeko(FILE * _Nonnull s, off_t offset, int whence)
{
    __flock(s);
    const int r = __fseeko(s, offset, whence);
    __funlock(s);

    return r;
}
