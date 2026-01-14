//
//  fopen_callbacks.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdlib.h>


FILE *fopen_callbacks(void* _Nullable _Restrict context, const FILE_Callbacks* _Nonnull _Restrict callbacks, const char* _Nonnull _Restrict mode)
{
    __FILE_Mode sm;

    if (__fopen_parse_mode(mode, &sm) == 0) {
        FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(FILE))) == NULL) {
            return NULL;
        }

        if (__fopen_init(self, context, callbacks, sm | __kStreamMode_FreeOnClose) == 0) {
            __freg_file((FILE*)self);
            return self;
        }

        free(self);
    }

    return NULL;
}
