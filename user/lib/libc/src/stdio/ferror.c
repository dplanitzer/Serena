//
//  ferror.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


void clearerr(FILE *_Nonnull s)
{
    __flock(s);
    s->flags.hasError = 0;
    s->flags.hasEof = 0;
    __funlock(s);
}

int feof(FILE * _Nonnull s)
{
    __flock(s);
    const int r = (s->flags.hasEof) ? EOF : 0;
    __funlock(s);

    return r;
}

int ferror(FILE * _Nonnull s)
{
    __flock(s);
    const int r = (s->flags.hasError) ? EOF : 0;
    __funlock(s);

    return r;
}
