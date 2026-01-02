//
//  fgetpos.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
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

    if (phys_pos < 0ll) {
        return (off_t)EOF;
    }


    if (s->flags.bufferMode > _IONBF) {
        switch (s->flags.direction) {
            case __kStreamDirection_Out:
                // physical file position is aligned with the start of the buffer
                return phys_pos + (off_t)s->bufferCount;

            case __kStreamDirection_In:
                // physical file position is aligned with the end of the buffer (bufferCount)
                return phys_pos - (off_t)(s->bufferCount - s->bufferIndex);

            case __kStreamDirection_Unknown:
                // buffer is guaranteed to be empty in this case
                return phys_pos;
        }
    }
    else {
        return phys_pos - (off_t)s->ugbCount;
    }
}


int fgetpos(FILE * _Nonnull _Restrict s, fpos_t * _Nonnull _Restrict pos)
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
