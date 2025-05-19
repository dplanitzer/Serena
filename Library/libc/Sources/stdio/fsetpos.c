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
    if (s->cb.seek == NULL) {
        errno = ESPIPE;
        return EOF;
    }

    if (s->flags.mostRecentDirection == __kStreamDirection_Write) {
        if (fflush(s) != 0) {
            return EOF;
        }
    }

    const long long r = s->cb.seek((void*)s->context, pos->offset, SEEK_SET);
    if (r < 0ll) {
        s->flags.hasError = 1;
        return EOF;
    }
    s->flags.hasEof = 0;
    // XXX drop ungetc buffered stuff

    return 0;
}
