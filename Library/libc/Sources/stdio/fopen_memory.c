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
    __FILE_Mode sm;

    if (__fopen_parse_mode(mode, &sm) == 0) {
        __Memory_FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(__Memory_FILE))) == NULL) {
            return NULL;
        }

        if (__fopen_memory_init(self, true, mem, sm) == 0) {
            return (FILE*)self;
        }

        free(self);
    }

    return NULL;
}
