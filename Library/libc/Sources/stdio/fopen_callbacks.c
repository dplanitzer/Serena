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
    FILE* self = NULL;

    try(__fopen_parse_mode(mode, &sm));
    try_null(self, malloc(SIZE_OF_FILE_SUBCLASS(FILE)), ENOMEM);
    try(__fopen_init(self, true, context, callbacks, sm));
    return self;

catch:
    free(self);
    errno = err;
    return NULL;
}
