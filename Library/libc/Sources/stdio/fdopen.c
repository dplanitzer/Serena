//
//  fdopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdlib.h>


FILE *fdopen(int ioc, const char * _Nonnull mode)
{
    __FILE_Mode sm;

    if (__fopen_parse_mode(mode, &sm) == 0) {
        __IOChannel_FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(__IOChannel_FILE))) == NULL) {
            return NULL;
        }

        if (__fdopen_init(self, true, ioc, sm) == 0) {
            __register_open_file((FILE*)self);
            return (FILE*)self;
        }

        free(self);
    }

    return NULL;
}
