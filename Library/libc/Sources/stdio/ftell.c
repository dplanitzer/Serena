//
//  ftell.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <limits.h>



long ftell(FILE *s)
{
    long long curpos;

    if (s->cb.seek == NULL) {
        errno = ESPIPE;
        return (long)EOF;
    }

    const errno_t err = s->cb.seek((void*)s->context, 0ll, &curpos, SEEK_CUR);
    if (err != 0) {
        s->flags.hasError = 1;
        errno = err;
        return (long)EOF;
    }

#if __LONG_WIDTH == 64
    return (long)curpos;
#else
    if (curpos > (long long)LONG_MAX) {
        errno = ERANGE;
        return EOF;
    } else {
        return (long)curpos;
    }
#endif
}
