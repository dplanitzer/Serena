//
//  fgetpos.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <limits.h>


// Returns the current logical stream position. This is the physical stream
// position adjusted by the current position in the buffer if buffering is
// enabled.
// Expects:
// - 's' is not NULL
// - 's' is seekable
static long long __fgetlogicalpos(FILE * _Nonnull s)
{
    const long long phys_pos = s->cb.seek((void*)s->context, 0ll, SEEK_CUR);

    if (phys_pos >= 0ll) {
        if (s->flags.bufferMode > _IONBF && s->flags.direction != __kStreamDirection_Unknown) {
            if (s->flags.direction == __kStreamDirection_Out) {
                return phys_pos + (long long)s->bufferCount;
            }
            else {
                return phys_pos - (long long)(s->bufferCount - s->bufferIndex);
            }
        }

        return phys_pos;
    }
    else {
        s->flags.hasError = 1;
        return (long long)EOF;
    }
}


int fgetpos(FILE *s, fpos_t *pos)
{
    __fensure_seekable(s);

    const long long r = __fgetlogicalpos(s);
    if (r >= 0ll) {
        pos->offset = r;
        pos->mbstate = s->mbstate;
        return 0;
    }
    else {
        return EOF;
    }
}

long ftell(FILE *s)
{
    __fensure_seekable(s);

    const long long r = __fgetlogicalpos(s);
    if (r < 0ll) {
        return (long)EOF;
    }

#if __LONG_WIDTH == 64
    return (long)r;
#else
    if (r > (long long)LONG_MAX) {
        errno = ERANGE;
        return EOF;
    } else {
        return (long)r;
    }
#endif
}
