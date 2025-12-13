//
//  fopen_null.c
//  libc
//
//  Created by Dietmar Planitzer on 1/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE * _Nullable __fopen_null(const char *mode)
{
    __FILE_Mode sm;

    if (__fopen_parse_mode(mode, &sm) == 0) {
        FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(FILE))) == NULL) {
            return NULL;
        }

        if (__fopen_null_init(self, true, sm) == 0) {
            return self;
        }

        free(self);
    }

    return NULL;
}
