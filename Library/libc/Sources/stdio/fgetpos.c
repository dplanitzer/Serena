//
//  fgetpos.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


int fgetpos(FILE *s, fpos_t *pos)
{
    if (s->cb.seek == NULL) {
        errno = ESPIPE;
        return EOF;
    }

    const long long r = s->cb.seek((void*)s->context, 0ll, SEEK_CUR);
    if (r >= 0ll) {
        pos->offset = r;
        return 0;
    }
    else {
        s->flags.hasError = 1;
        return EOF;
    }
}
