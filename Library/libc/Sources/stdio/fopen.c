//
//  fopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *fopen(const char *filename, const char *mode)
{
    decl_try_err();
    __FILE_Mode sm;
    __IOChannel_FILE* self = NULL;

    try(__fopen_parse_mode(mode, &sm));
    try_null(self, malloc(SIZE_OF_FILE_SUBCLASS(__IOChannel_FILE)), ENOMEM);
    try(__fopen_filename_init(self, filename, sm));
    return (FILE*)self;

catch:
    free(self);
    errno = err;
    return NULL;
}
