//
//  Stream_Null.c
//  Apollo
//
//  Created by Dietmar Planitzer on 1/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>
#include <string.h>
#include <apollo/apollo.h>

// This a non-seekable stream that discards anything that is written to it and
// returns EOF on read.
// Its used ie by the printf() implementation it the case where the user just
// wants to know how long the formatter string would be but they don't want the
// actual data.


static errno_t __null_read(void* _Nonnull self, void* pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    *pOutBytesRead = 0;
    return 0;
}

static errno_t __null_write(void* _Nonnull self, const void* pBytes, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    *pOutBytesWritten = nBytesToWrite;
    return 0;
}

static const FILE_Callbacks __FILE_null_callbacks = {
    (FILE_Read)__null_read,
    (FILE_Write)__null_write,
    (FILE_Seek)NULL,
    (FILE_Close)NULL
};



errno_t __fopen_null_init(FILE* _Nonnull self, const char *mode)
{
    return __fopen_init(self, true, NULL, &__FILE_null_callbacks, mode);
}

FILE *__fopen_null(const char *mode)
{
    decl_try_err();
    FILE* self = NULL;

    try_null(self, malloc(SIZE_OF_FILE_SUBCLASS(FILE)), ENOMEM);
    try(__fopen_null_init(self, mode));

    return self;

catch:
    free(self);
    errno = err;
    return NULL;
}
