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
    __FILE_Mode sm;

    if ((err = __fopen_parse_mode(mode, &sm)) == EOK) {
        __Memory_FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(__Memory_FILE))) == NULL) {
            return NULL;
        }

        if ((err = __fopen_memory_init(self, true, mem, sm)) == EOK) {
            return (FILE*)self;
        }

        free(self);
    }

    errno = err;
    return NULL;
}
