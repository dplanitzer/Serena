//
//  fsetpos.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


int fsetpos(FILE * _Nonnull _Restrict s, const fpos_t * _Nonnull _Restrict pos)
{
    int r = EOF;

    __flock(s);
    __fensure_seekable(s);
    __fensure_direction(s, __kStreamDirection_Unknown);

    const long long res = s->cb.seek((void*)s->context, pos->offset, SEEK_SET);
    if (res >= 0ll) {
        __fdiscard_ugb(s);
        s->flags.hasEof = 0;
        s->mbstate = pos->mbstate;
        r = 0;
    }
    else {
        s->flags.hasError = 1;
    }

catch:
    __funlock(s);
    return r;
}
