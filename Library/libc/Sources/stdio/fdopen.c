//
//  fdopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *fdopen(int ioc, const char *mode)
{
    decl_try_err();
    __IOChannel_FILE* self = NULL;

    try_null(self, malloc(SIZE_OF_FILE_SUBCLASS(__IOChannel_FILE)), ENOMEM);
    try(__fdopen_init(self, true, ioc, mode));
    return (FILE*)self;

catch:
    free(self);
    errno = err;
    return NULL;
}
