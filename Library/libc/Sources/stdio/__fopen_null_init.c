//
//  __fopen_null_init.c
//  libc
//
//  Created by Dietmar Planitzer on 1/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


// This a non-seekable stream that discards anything that is written to it and
// returns EOF on read.
// It's used ie by the printf() implementation in the case where the user just
// wants to know how long the formatted string would be but they don't want the
// actual data.

static ssize_t __null_read(void* _Nonnull self, void* pBuffer, ssize_t nBytesToRead)
{
    return 0;
}

static ssize_t __null_write(void* _Nonnull self, const void* pBytes, ssize_t nBytesToWrite)
{
    return nBytesToWrite;
}

static const FILE_Callbacks __FILE_null_callbacks = {
    (FILE_Read)__null_read,
    (FILE_Write)__null_write,
    (FILE_Seek)NULL,
    (FILE_Close)NULL
};



int __fopen_null_init(FILE* _Nonnull self, bool bFreeOnClose, __FILE_Mode sm)
{
    return __fopen_init(self, bFreeOnClose, NULL, &__FILE_null_callbacks, sm);
}
