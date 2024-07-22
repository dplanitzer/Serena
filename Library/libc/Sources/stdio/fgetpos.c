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

    const errno_t err = s->cb.seek((void*)s->context, 0ll, &pos->offset, SEEK_CUR);
    if (err != 0) {
        s->flags.hasError = 1;
        errno = err;
        return EOF;
    }

    return 0;
}
