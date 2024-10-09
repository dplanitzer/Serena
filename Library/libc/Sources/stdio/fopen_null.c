//
//  fopen_null.c
//  libc
//
//  Created by Dietmar Planitzer on 1/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *__fopen_null(const char *mode)
{
    decl_try_err();
    __FILE_Mode sm;

    if ((err = __fopen_parse_mode(mode, &sm)) == EOK) {
        FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(FILE))) == NULL) {
            return NULL;
        }

        if ((err = __fopen_null_init(self, true, sm)) == EOK) {
            return self;
        }

        free(self);
    }

    errno = err;
    return NULL;
}
