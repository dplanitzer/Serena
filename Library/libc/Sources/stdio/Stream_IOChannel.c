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
    return IOChannel_Read(self->ioc, pBuffer, nBytesToRead, pOutBytesRead);
}

static errno_t __ioc_write(__IOChannel_FILE_Vars* _Nonnull self, const void* pBytes, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    return IOChannel_Write(self->ioc, pBytes, nBytesToWrite, pOutBytesWritten);
}

static errno_t __ioc_seek(__IOChannel_FILE_Vars* _Nonnull self, long long offset, long long *outOldOffset, int whence)
{
    return File_Seek(self->ioc, offset, outOldOffset, whence);
}

static errno_t __ioc_close(__IOChannel_FILE_Vars* _Nonnull self)
{
    return IOChannel_Close(self->ioc);
}

static const FILE_Callbacks __FILE_ioc_callbacks = {
    (FILE_Read)__ioc_read,
    (FILE_Write)__ioc_write,
    (FILE_Seek)__ioc_seek,
    (FILE_Close)__ioc_close
};



errno_t __fdopen_init(__IOChannel_FILE* _Nonnull self, bool bFreeOnClose, int ioc, const char *mode)
{
    __FILE_Mode sm;
    const errno_t err = __fopen_parse_mode(mode, &sm);
    if (err != EOK) {
        return err;
    }

    // The I/O channel must be valid and open
    const int iocmode = IOChannel_GetMode(ioc);
    if (iocmode == 0) {
        return EBADF;
    }

    // Make sure that 'mode' lines up with what the I/O channel can actually
    // do
    if (((sm & __kStreamMode_Read) != 0) && ((iocmode & kOpen_Read) == 0)) {
        return EINVAL;
    }
    if (((sm & __kStreamMode_Write) != 0) && ((iocmode & kOpen_Write) == 0)) {
        return EINVAL;
    }
    if (((sm & __kStreamMode_Append) != 0) && ((iocmode & kOpen_Append) == 0)) {
        return EINVAL;
    }

    self->v.ioc = ioc;
    return __fopen_init((FILE*)self, false, &self->v, &__FILE_ioc_callbacks, mode);
}


errno_t __fopen_filename_init(__IOChannel_FILE* _Nonnull self, const char *filename, const char *mode)
{
    decl_try_err();
    int options = 0;
    int ioc = -1;
    __FILE_Mode sm;


    try(__fopen_parse_mode(mode, &sm));

    if ((sm & __kStreamMode_Read) != 0) {
        options |= kOpen_Read;
    }
    if ((sm & __kStreamMode_Write) != 0) {
        options |= kOpen_Write;
    }
    if ((sm & __kStreamMode_Append) != 0) {
        options |= kOpen_Append;
    }
    if ((sm & __kStreamMode_Truncate) != 0) {
        options |= kOpen_Truncate;
    }
    if ((sm & __kStreamMode_Exclusive) != 0) {
        options |= kOpen_Exclusive;
    }


    // Open/create the file
    if ((sm & __kStreamMode_Create) == __kStreamMode_Create) {
        try(File_Create(filename, options, 0666, &ioc));
    }
    else {
        try(File_Open(filename, options, &ioc));
    }
    
    self->v.ioc = ioc;
    try(__fopen_init((FILE*)self, true, &self->v, &__FILE_ioc_callbacks, mode));


    // Make sure that the return value of ftell() issued before the first write
    // lines up with the actual end of file position.
    if ((sm & __kStreamMode_Append) != 0) {
        self->super.cb.seek(self->super.context, 0ll, NULL, SEEK_END);
    }

    return EOK;

catch:
    if (ioc != -1) {
        IOChannel_Close(ioc);
    }
    return err;
}

int fileno(FILE *s)
{
    return (s->cb.read == (FILE_Read)__ioc_read) ? (int)s->context : EOF;
}
