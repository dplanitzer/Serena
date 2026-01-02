//
//  __fdopen_init.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <fcntl.h>


ssize_t __fd_read(__IOChannel_FILE_Vars* _Nonnull self, void* buf, ssize_t nbytes)
{
    return read(self->fd, buf, nbytes);
}

static ssize_t __fd_write(__IOChannel_FILE_Vars* _Nonnull self, const void* buf, ssize_t nbytes)
{
    return write(self->fd, buf, nbytes);
}

static long long __fd_seek(__IOChannel_FILE_Vars* _Nonnull self, long long offset, int whence)
{
    const off_t r = lseek(self->fd, offset, whence);

    if (r >= 0ll) {
        return r;
    } else {
        return EOF;
    }
}

static int __fd_close(__IOChannel_FILE_Vars* _Nonnull self)
{
    return (close(self->fd) == 0) ? 0 : EOF;
}

const FILE_Callbacks __FILE_fd_callbacks = {
    (FILE_Read)__fd_read,
    (FILE_Write)__fd_write,
    (FILE_Seek)__fd_seek,
    (FILE_Close)__fd_close
};



int __fdopen_init(__IOChannel_FILE* _Nonnull self, bool bFreeOnClose, int fd, __FILE_Mode sm)
{
    // The descriptor must be valid and open
    const int fl = fcntl(fd, F_GETFL);
    
    if (fl == -1) {
        errno = EBADF;
        return EOF;
    }


    // Make sure that 'mode' lines up with what the descriptor can actually
    // do
    int ok = 1;

    if (((sm & __kStreamMode_Read) != 0) && ((fl & O_RDONLY) == 0)) {
        ok = 0;
    }
    if (((sm & __kStreamMode_Write) != 0) && ((fl & O_WRONLY) == 0)) {
        ok = 0;
    }
    if (((sm & __kStreamMode_Append) != 0) && ((fl & O_APPEND) == 0)) {
        ok = 0;
    }
    if (!ok) {
        errno = EINVAL;
        return EOF;
    }


    self->v.fd = fd;
    return __fopen_init((FILE*)self, bFreeOnClose, &self->v, &__FILE_fd_callbacks, sm);
}
