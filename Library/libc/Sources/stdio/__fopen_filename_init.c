//
//  __fopen_filename_init.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <fcntl.h>
#include <stdlib.h>


int __fopen_filename_init(__IOChannel_FILE* _Nonnull _Restrict self, bool bFreeOnClose, const char * _Nonnull _Restrict filename, __FILE_Mode sm)
{
    int oflags = 0;

    if ((sm & __kStreamMode_Read) != 0) {
        oflags |= O_RDONLY;
    }
    if ((sm & __kStreamMode_Write) != 0) {
        oflags |= O_WRONLY;
    }
    if ((sm & __kStreamMode_Append) != 0) {
        oflags |= O_APPEND;
    }
    if ((sm & __kStreamMode_Truncate) != 0) {
        oflags |= O_TRUNC;
    }
    if ((sm & __kStreamMode_Exclusive) != 0) {
        oflags |= O_EXCL;
    }
    if ((sm & __kStreamMode_Create) != 0) {
        oflags |= O_CREAT;
    }


    // Open/create the file
    const int fd = open(filename, oflags, 0666);
    if (fd < 0) {
        return EOF;
    }
    
    self->v.fd = fd;
    if (__fopen_init((FILE*)self, bFreeOnClose, &self->v, &__FILE_fd_callbacks, sm) != 0) {
        close(fd);
        return EOF;
    }


    // Make sure that the return value of ftell() issued before the first write
    // lines up with the actual end of file position.
    if ((sm & __kStreamMode_Append) != 0) {
        self->super.cb.seek(self->super.context, 0ll, SEEK_END);
    }

    return 0;
}
