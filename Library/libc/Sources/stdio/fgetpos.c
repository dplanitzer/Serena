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
off_t __fgetlogicalpos(FILE * _Nonnull s)
{
    const off_t phys_pos = s->cb.seek(s->context, 0ll, SEEK_CUR);

    if (phys_pos >= 0ll) {
        if (s->flags.bufferMode > _IONBF && s->flags.direction != __kStreamDirection_Unknown) {
            if (s->flags.direction == __kStreamDirection_Out) {
                return phys_pos + (off_t)s->bufferCount;
            }
            else {
                return phys_pos - (off_t)(s->bufferCount - s->bufferIndex);
            }
        }

        return phys_pos;
    }
    else {
        return (off_t)EOF;
    }
}


int fgetpos(FILE *s, fpos_t *pos)
{
    __fensure_seekable(s, EOF);

    const off_t r = __fgetlogicalpos(s);
    if (r >= 0ll) {
        pos->offset = r;
        pos->mbstate = s->mbstate;
        return 0;
    }
    else {
        s->flags.hasError = 1;
        return EOF;
    }
}
