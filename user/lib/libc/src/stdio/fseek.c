//
//  fseek.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


int fseek(FILE * _Nonnull s, long offset, int whence)
{
    __flock(s);
    const int r = __fseeko(s, (off_t)offset, whence);
    __funlock(s);

    return r;
}
