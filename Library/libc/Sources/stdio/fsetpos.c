//
//  fsetpos.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


int fsetpos(FILE *s, const fpos_t *pos)
{
    __fensure_seekable(s, EOF);
    __fensure_direction(s, __kStreamDirection_Unknown, EOF);

    const long long r = s->cb.seek((void*)s->context, pos->offset, SEEK_SET);
    if (r < 0ll) {
        s->flags.hasError = 1;
        return EOF;
    }
    __fdiscard_ugb(s);
    s->flags.hasEof = 0;
    s->mbstate = pos->mbstate;

    return 0;
}
