//
//  ftello.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <limits.h>


off_t ftello(FILE * _Nonnull s)
{
    off_t r = (off_t)EOF;

    __flock(s);
    __fensure_seekable(s);

    const off_t lp = __fgetlogicalpos(s);
    if (lp >= 0) {
        r = lp;
    }
    else {
        s->flags.hasError = 1;
    }

catch:
    __funlock(s);
    return r;
}
