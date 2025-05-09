//
//  Stream_IOChannel.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <System/System.h>


static errno_t __ioc_read(__IOChannel_FILE_Vars* _Nonnull self, void* pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    return read(self->ioc, pBuffer, nBytesToRead, pOutBytesRead);
}

static errno_t __ioc_write(__IOChannel_FILE_Vars* _Nonnull self, const void* pBytes, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    return write(self->ioc, pBytes, nBytesToWrite, pOutBytesWritten);
}

static errno_t __ioc_seek(__IOChannel_FILE_Vars* _Nonnull self, long long offset, long long *pOutOldOffset, int whence)
{
    return seek(self->ioc, offset, pOutOldOffset, whence);
}

static errno_t __ioc_close(__IOChannel_FILE_Vars* _Nonnull self)
{
    return close(self->ioc);
}

static const FILE_Callbacks __FILE_ioc_callbacks = {
    (FILE_Read)__ioc_read,
    (FILE_Write)__ioc_write,
    (FILE_Seek)__ioc_seek,
    (FILE_Close)__ioc_close
};



errno_t __fdopen_init(__IOChannel_FILE* _Nonnull self, bool bFreeOnClose, int ioc, __FILE_Mode sm)
{
    // The I/O channel must be valid and open
    const int iocmode = fgetmode(ioc);
    if (iocmode == 0) {
        return EBADF;
    }

    // Make sure that 'mode' lines up with what the I/O channel can actually
    // do
    if (((sm & __kStreamMode_Read) != 0) && ((iocmode & O_RDONLY) == 0)) {
        return EINVAL;
    }
    if (((sm & __kStreamMode_Write) != 0) && ((iocmode & O_WRONLY) == 0)) {
        return EINVAL;
    }
    if (((sm & __kStreamMode_Append) != 0) && ((iocmode & O_APPEND) == 0)) {
        return EINVAL;
    }

    self->v.ioc = ioc;
    return __fopen_init((FILE*)self, bFreeOnClose, &self->v, &__FILE_ioc_callbacks, sm);
}


errno_t __fopen_filename_init(__IOChannel_FILE* _Nonnull self, bool bFreeOnClose, const char *filename, __FILE_Mode sm)
{
    decl_try_err();
    unsigned int options = 0;
    int ioc = -1;

    if ((sm & __kStreamMode_Read) != 0) {
        options |= O_RDONLY;
    }
    if ((sm & __kStreamMode_Write) != 0) {
        options |= O_WRONLY;
    }
    if ((sm & __kStreamMode_Append) != 0) {
        options |= O_APPEND;
    }
    if ((sm & __kStreamMode_Truncate) != 0) {
        options |= O_TRUNC;
    }
    if ((sm & __kStreamMode_Exclusive) != 0) {
        options |= O_EXCL;
    }


    // Open/create the file
    if ((sm & __kStreamMode_Create) == __kStreamMode_Create) {
        try(mkfile(filename, options, 0666, &ioc));
    }
    else {
        try(open(filename, options, &ioc));
    }
    
    self->v.ioc = ioc;
    try(__fopen_init((FILE*)self, bFreeOnClose, &self->v, &__FILE_ioc_callbacks, sm));


    // Make sure that the return value of ftell() issued before the first write
    // lines up with the actual end of file position.
    if ((sm & __kStreamMode_Append) != 0) {
        self->super.cb.seek(self->super.context, 0ll, NULL, SEEK_END);
    }

    return EOK;

catch:
    if (ioc != -1) {
        close(ioc);
    }
    return err;
}

int fileno(FILE *s)
{
    return (s->cb.read == (FILE_Read)__ioc_read) ? ((__IOChannel_FILE*)s)->v.ioc : EOF;
}
