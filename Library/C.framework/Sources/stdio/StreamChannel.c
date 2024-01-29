//
//  Stream.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
//#include <limits.h>
//#include <stdlib.h>
#include <apollo/apollo.h>


static errno_t ioc_read(struct __FILE_fdopen* self, void* pBuffer, ssize_t nBytesToRead, ssize_t* pOutBytesRead)
{
    ssize_t bytesRead = read(self->fd, pBuffer, nBytesToRead);

    if (bytesRead >= 0) {
        *pOutBytesRead = bytesRead;
        return 0;
    } else {
        *pOutBytesRead = 0;
        return (errno_t) -bytesRead;
    }
}

static errno_t ioc_write(struct __FILE_fdopen* self, const void* pBytes, ssize_t nBytesToWrite, ssize_t* pOutBytesWritten)
{
    ssize_t bytesWritten = write(self->fd, pBytes, nBytesToWrite);

    if (bytesWritten >= 0) {
        *pOutBytesWritten = bytesWritten;
        return 0;
    } else {
        *pOutBytesWritten = 0;
        return (errno_t) -bytesWritten;
    }
}

static errno_t ioc_seek(struct __FILE_fdopen* self, long long offset, long long *outOldOffset, int whence)
{
    return seek(self->fd, (off_t)offset, outOldOffset, whence);
}

static errno_t ioc_close(struct __FILE_fdopen* self)
{
    return close(self->fd);
}



errno_t __fdopen_init(FILE* self, int ioc, const char *mode)
{
    decl_try_err();
    const int sm = __fopen_parse_mode(mode);

    if (sm == 0) {
        throw(EINVAL);
    }

    try(__fopen_init(self, (__FILE_read)ioc_read, (__FILE_write)ioc_write, (__FILE_seek)ioc_seek, (__FILE_close)ioc_close, sm));
    self->u.ioc.fd = ioc;
    return 0;

catch:
    errno = err;
    return err;
}

FILE *fdopen(int ioc, const char *mode)
{
    decl_try_err();
    FILE* self = NULL;
    const int sm = __fopen_parse_mode(mode);

    if (sm == 0) {
        throw(EINVAL);
    }

    try_null(self, __fopen((__FILE_read)ioc_read, (__FILE_write)ioc_write, (__FILE_seek)ioc_seek, (__FILE_close)ioc_close, sm), ENOMEM);
    self->u.ioc.fd = ioc;

    return self;

catch:
    errno = err;
    return NULL;
}

FILE *fopen(const char *filename, const char *mode)
{
    decl_try_err();
    FILE* self = NULL;
    const int sm = __fopen_parse_mode(mode);
    int fd = -1;

    int options = 0;
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

    if (options == 0) {
        throw(EINVAL);
    }


    // Open/create the file
    if ((sm & __kStreamMode_Write) == 0) {
        // Read only
        try(open(filename, options, &fd));
    }
    else {
        // Write only or read/write/append
        try(creat(filename, options, 0666, &fd));
    }
    
    try_null(self, __fopen((__FILE_read)ioc_read, (__FILE_write)ioc_write, (__FILE_seek)ioc_seek, (__FILE_close)ioc_close, sm), ENOMEM);
    self->u.ioc.fd = fd;


    // Make sure that the return value of ftell() lines up with the actual
    // end of file position.
    if ((sm & __kStreamMode_Append) != 0) {
        self->seekfn(&self->u, 0ll, NULL, SEEK_END);
    }

    return self;

catch:
    if (fd != -1) {
        close(fd);
    }
    errno = err;
    return NULL;
}

int fileno(FILE *s)
{
    return (s->readfn == (__FILE_read)ioc_read) ? s->u.ioc.fd : EOF;
}
