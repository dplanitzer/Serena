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
    __FILE_Mode sm;

    if (__fopen_parse_mode(mode, &sm) == 0) {
        FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(FILE))) == NULL) {
            return NULL;
        }

        if (__fopen_init(self, true, context, callbacks, sm) == 0) {
            return self;
        }

        free(self);
    }

    return NULL;
}
