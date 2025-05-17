//
//  Stream_IOChannel.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>


static errno_t __fd_read(__IOChannel_FILE_Vars* _Nonnull self, void* pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    return read(self->ioc, pBuffer, nBytesToRead, pOutBytesRead);
}

static errno_t __fd_write(__IOChannel_FILE_Vars* _Nonnull self, const void* pBytes, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    return write(self->ioc, pBytes, nBytesToWrite, pOutBytesWritten);
}

static errno_t __fd_seek(__IOChannel_FILE_Vars* _Nonnull self, long long offset, long long *pOutOldOffset, int whence)
{
    const off_t r = lseek(self->ioc, offset, whence);

    if (r >= 0ll) {
        *pOutOldOffset = r;
        return EOK;
    } else {
        *pOutOldOffset = 0ll;
        return errno;
    }
}

static errno_t __fd_close(__IOChannel_FILE_Vars* _Nonnull self)
{
    return (close(self->ioc) == 0) ? 0 : errno;
}

static const FILE_Callbacks __FILE_fd_callbacks = {
    (FILE_Read)__fd_read,
    (FILE_Write)__fd_write,
    (FILE_Seek)__fd_seek,
    (FILE_Close)__fd_close
};



errno_t __fdopen_init(__IOChannel_FILE* _Nonnull self, bool bFreeOnClose, int fd, __FILE_Mode sm)
{
    // The descriptor must be valid and open
    const int fl = fcntl(fd, F_GETFL);
    if (fl == -1) {
        return EBADF;
    }

    // Make sure that 'mode' lines up with what the descriptor can actually
    // do
    if (((sm & __kStreamMode_Read) != 0) && ((fl & O_RDONLY) == 0)) {
        return EINVAL;
    }
    if (((sm & __kStreamMode_Write) != 0) && ((fl & O_WRONLY) == 0)) {
        return EINVAL;
    }
    if (((sm & __kStreamMode_Append) != 0) && ((fl & O_APPEND) == 0)) {
        return EINVAL;
    }

    self->v.ioc = fd;
    return __fopen_init((FILE*)self, bFreeOnClose, &self->v, &__FILE_fd_callbacks, sm);
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
        try(creat(filename, options, 0666, &ioc));
    }
    else {
        try(open(filename, options, &ioc));
    }
    
    self->v.ioc = ioc;
    try(__fopen_init((FILE*)self, bFreeOnClose, &self->v, &__FILE_fd_callbacks, sm));


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
    return (s->cb.read == (FILE_Read)__fd_read) ? ((__IOChannel_FILE*)s)->v.ioc : EOF;
}
