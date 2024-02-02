//
//  Stream_IOChannel.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <apollo/apollo.h>


static errno_t __ioc_read(__IOChannel_FILE_Vars* _Nonnull self, void* pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    ssize_t bytesRead = read(self->ioc, pBuffer, nBytesToRead);

    if (bytesRead >= 0) {
        *pOutBytesRead = bytesRead;
        return 0;
    } else {
        *pOutBytesRead = 0;
        return (errno_t) -bytesRead;
    }
}

static errno_t __ioc_write(__IOChannel_FILE_Vars* _Nonnull self, const void* pBytes, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    ssize_t bytesWritten = write(self->ioc, pBytes, nBytesToWrite);

    if (bytesWritten >= 0) {
        *pOutBytesWritten = bytesWritten;
        return 0;
    } else {
        *pOutBytesWritten = 0;
        return (errno_t) -bytesWritten;
    }
}

static errno_t __ioc_seek(__IOChannel_FILE_Vars* _Nonnull self, long long offset, long long *outOldOffset, int whence)
{
    return seek(self->ioc, offset, outOldOffset, whence);
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



errno_t __fdopen_init(__IOChannel_FILE* _Nonnull self, bool bFreeOnClose, int ioc, const char *mode)
{
    const int sm = __fopen_parse_mode(mode);
    const int iocmode = fgetmode(ioc);

    // Make sure that 'mode' lines up with what the I/O channel can actually
    // do
    if ((sm & __kStreamMode_Read) != 0 && (iocmode & O_RDONLY) == 0) {
        return EINVAL;
    }
    if ((sm & __kStreamMode_Write) != 0 && (iocmode & O_WRONLY) == 0) {
        return EINVAL;
    }
    if ((sm & __kStreamMode_Append) != 0 && (iocmode & O_APPEND) == 0) {
        return EINVAL;
    }

    self->v.ioc = ioc;
    return __fopen_init((FILE*)self, false, &self->v, &__FILE_ioc_callbacks, mode);
}

FILE *fdopen(int ioc, const char *mode)
{
    decl_try_err();
    __IOChannel_FILE* self = NULL;

    try_null(self, malloc(SIZE_OF_FILE_SUBCLASS(__IOChannel_FILE)), ENOMEM);
    try(__fdopen_init(self, true, ioc, mode));
    return (FILE*)self;

catch:
    free(self);
    errno = err;
    return NULL;
}

errno_t __fopen_filename_init(__IOChannel_FILE* _Nonnull self, const char *filename, const char *mode)
{
    decl_try_err();
    int options = 0;
    int ioc = -1;
    const int sm = __fopen_parse_mode(mode);

    if ((sm & __kStreamMode_Read) != 0) {
        options |= O_RDONLY;
    }
    if ((sm & __kStreamMode_Write) != 0) {
        options |= O_WRONLY;
        if ((sm & __kStreamMode_Append) != 0) {
            options |= O_APPEND;
        } else {
            options |= O_TRUNC;
        }
        if ((sm & __kStreamMode_Exclusive) != 0) {
            options |= O_EXCL;
        }
    }

    if ((options & O_RDWR) == 0) {
        throw(EINVAL);
    }


    // Open/create the file
    if ((sm & __kStreamMode_Write) == 0) {
        // Read only
        try(open(filename, options, &ioc));
    }
    else {
        // Write only or read/write/append
        try(creat(filename, options, 0666, &ioc));
    }
    
    self->v.ioc = ioc;
    try(__fopen_init((FILE*)self, true, &self->v, &__FILE_ioc_callbacks, mode));


    // Make sure that the return value of ftell() issued before the first write
    // lines up with the actual end of file position.
    if ((sm & __kStreamMode_Append) != 0) {
        self->super.cb.seek(self->super.context, 0ll, NULL, SEEK_END);
    }

    return 0;

catch:
    if (ioc != -1) {
        close(ioc);
    }
    return err;
}

FILE *fopen(const char *filename, const char *mode)
{
    decl_try_err();
    __IOChannel_FILE* self = NULL;

    try_null(self, malloc(SIZE_OF_FILE_SUBCLASS(__IOChannel_FILE)), ENOMEM);
    try(__fopen_filename_init(self, filename, mode));
    return (FILE*)self;

catch:
    free(self);
    errno = err;
    return NULL;
}

FILE *freopen(const char *filename, const char *mode, FILE *s)
{
    if ((__fopen_parse_mode(mode) & (__kStreamMode_Read|__kStreamMode_Write)) == 0) {
        fclose(s);
        return NULL;
    }

    __fclose(s);
    const errno_t err = __fopen_filename_init((__IOChannel_FILE*)s, filename, mode);
    if (err != 0) {
        errno = err;
        return NULL;
    }
    else {
        return s;
    }
}

int fileno(FILE *s)
{
    return (s->cb.read == (FILE_Read)__ioc_read) ? (int)s->context : EOF;
}
