//
//  fdreopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *fdreopen(int ioc, const char * _Nonnull _Restrict mode, FILE * _Nonnull _Restrict s)
{
    const bool isFreeOnClose = s->flags.shouldFreeOnClose;
    __FILE_Mode sm;
    
    if (__fopen_parse_mode(mode, &sm) == 0) {
        __fclose(s);

        if (__fdopen_init((__IOChannel_FILE*)s, isFreeOnClose, ioc, sm) == 0) {
            return s;
        }
    }

    return NULL;
}
