//
//  ftell.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <limits.h>


off_t ftello(FILE * _Nonnull s)
{
    __fensure_seekable(s, EOF);

    const off_t r = __fgetlogicalpos(s);
    if (r >= 0) {
        return r;
    }
    else {
        s->flags.hasError = 1;
        return (off_t)EOF;
    }
}

long ftell(FILE * _Nonnull s)
{
    __fensure_seekable(s, EOF);

    const off_t r = __fgetlogicalpos(s);
    if (r < 0ll) {
        s->flags.hasError = 1;
        return (long)EOF;
    }

#if __LONG_WIDTH == 64
    return (long)r;
#else
    if (r > (off_t)LONG_MAX) {
        errno = ERANGE;
        s->flags.hasError = 1;
        return EOF;
    } else {
        return (long)r;
    }
#endif
}
