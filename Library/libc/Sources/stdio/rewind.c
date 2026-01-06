//
//  rewind.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


void rewind(FILE * _Nonnull s)
{
    __flock(s);
    (void) __fseeko(s, 0ll, SEEK_SET);
    s->flags.hasError = 0;
    __funlock(s);
}
