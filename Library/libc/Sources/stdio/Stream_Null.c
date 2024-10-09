//
//  Stream_Null.c
//  libc
//
//  Created by Dietmar Planitzer on 1/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


// This a non-seekable stream that discards anything that is written to it and
// returns EOF on read.
// It's used ie by the printf() implementation in the case where the user just
// wants to know how long the formatted string would be but they don't want the
// actual data.

static errno_t __null_read(void* _Nonnull self, void* pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    *pOutBytesRead = 0;
    return EOK;
}

static errno_t __null_write(void* _Nonnull self, const void* pBytes, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    *pOutBytesWritten = nBytesToWrite;
    return EOK;
}

static const FILE_Callbacks __FILE_null_callbacks = {
    (FILE_Read)__null_read,
    (FILE_Write)__null_write,
    (FILE_Seek)NULL,
    (FILE_Close)NULL
};



errno_t __fopen_null_init(FILE* _Nonnull self, bool bFreeOnClose, __FILE_Mode sm)
{
    return __fopen_init(self, bFreeOnClose, NULL, &__FILE_null_callbacks, sm);
}
