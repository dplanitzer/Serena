//
//  fopen_memory.c
//  libc
//
//  Created by Dietmar Planitzer on 1/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *fopen_memory(FILE_Memory *mem, const char *mode)
{
    decl_try_err();
    __Memory_FILE* self = NULL;

    try_null(self, malloc(SIZE_OF_FILE_SUBCLASS(__Memory_FILE)), ENOMEM);
    try(__fopen_memory_init(self, mem, mode));

    return (FILE*)self;

catch:
    free(self);
    errno = err;
    return NULL;
}
