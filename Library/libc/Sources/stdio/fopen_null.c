//
//  fopen_null.c
//  libc
//
//  Created by Dietmar Planitzer on 1/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *__fopen_null(const char *mode)
{
    decl_try_err();
    __FILE_Mode sm;
    FILE* self = NULL;

    try(__fopen_parse_mode(mode, &sm));
    try_null(self, malloc(SIZE_OF_FILE_SUBCLASS(FILE)), ENOMEM);
    try(__fopen_null_init(self, sm));

    return self;

catch:
    free(self);
    errno = err;
    return NULL;
}
