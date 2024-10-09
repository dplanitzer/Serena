//
//  fopen_callbacks.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *fopen_callbacks(void* context, const FILE_Callbacks* callbacks, const char* mode)
{
    decl_try_err();
    __FILE_Mode sm;

    if ((err = __fopen_parse_mode(mode, &sm)) == EOK) {
        FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(FILE))) == NULL) {
            return NULL;
        }

        if ((err = __fopen_init(self, true, context, callbacks, sm)) == EOK) {
            return self;
        }

        free(self);
    }

    errno = err;
    return NULL;
}
