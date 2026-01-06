//
//  freopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdlib.h>


FILE *freopen(const char * _Nonnull _Restrict filename, const char * _Nonnull _Restrict mode, FILE * _Nonnull _Restrict s)
{
    __FILE_Mode sm;

    if (__fopen_parse_mode(mode, &sm) != 0) {
        return NULL;
    }

    __flock(s);
    const bool isFreeOnClose = s->flags.shouldFreeOnClose;
    __fclose(s);

    const int r = __fopen_filename_init((__IOChannel_FILE*)s, isFreeOnClose, filename, sm);
    __funlock(s);
    
    return (r == 0) ? s : NULL;
}
