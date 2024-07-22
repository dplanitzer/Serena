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
    FILE* self = NULL;

    try_null(self, malloc(SIZE_OF_FILE_SUBCLASS(FILE)), ENOMEM);
    try(__fopen_init(self, true, context, callbacks, mode));
    return self;

catch:
    free(self);
    errno = err;
    return NULL;
}
