//
//  fileno.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


int fileno(FILE * _Nonnull s)
{
    __flock(s);
    const int r = (s->cb.read == (FILE_Read)__fd_read) ? ((__FD_FILE*)s)->v.fd : EOF;
    __funlock(s);

    return r;
}
