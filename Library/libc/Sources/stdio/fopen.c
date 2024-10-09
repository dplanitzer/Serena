//
//  fopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *fopen(const char *filename, const char *mode)
{
    decl_try_err();
    __FILE_Mode sm;

    if ((err = __fopen_parse_mode(mode, &sm)) == EOK) {
        __IOChannel_FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(__IOChannel_FILE))) == NULL) {
            return NULL;
        }

        if ((err = __fopen_filename_init(self, true, filename, sm)) == EOK) {
            return (FILE*)self;
        }

        free(self);
    }

    errno = err;
    return NULL;
}
