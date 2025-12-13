//
//  fopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *fopen(const char * _Nonnull _Restrict filename, const char * _Nonnull _Restrict mode)
{
    __FILE_Mode sm;

    if (__fopen_parse_mode(mode, &sm) == 0) {
        __IOChannel_FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(__IOChannel_FILE))) == NULL) {
            return NULL;
        }

        if (__fopen_filename_init(self, true, filename, sm) == 0) {
            return (FILE*)self;
        }

        free(self);
    }

    return NULL;
}
