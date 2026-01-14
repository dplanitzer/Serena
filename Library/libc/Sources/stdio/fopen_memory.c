//
//  fopen_memory.c
//  libc
//
//  Created by Dietmar Planitzer on 1/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdlib.h>


FILE *fopen_memory(FILE_Memory * _Nonnull _Restrict mem, const char * _Nonnull _Restrict mode)
{
    __FILE_Mode sm;

    if (__fopen_parse_mode(mode, &sm) == 0) {
        __Memory_FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(__Memory_FILE))) == NULL) {
            return NULL;
        }

        if (__fopen_memory_init(self, mem, sm | __kStreamMode_FreeOnClose) == 0) {
            __register_open_file((FILE*)self);
            return (FILE*)self;
        }

        free(self);
    }

    return NULL;
}
