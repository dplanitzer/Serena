//
//  __fopen_filename_init.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdlib.h>
#include <serena/file.h>

int __fopen_filename_init(__IOChannel_FILE* _Nonnull _Restrict self, const char * _Nonnull _Restrict filename, __FILE_Mode sm)
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


    // Open/create the file
    int fd;
    if ((sm & __kStreamMode_Create) != 0) {
        fd = fs_create_file(NULL, filename, oflags, 0666);
    }
    else {
        fd = fs_open(NULL, filename, oflags);
    }
    if (fd < 0) {
        return EOF;
    }
    
    self->v.fd = fd;
    if (__fopen_init((FILE*)self, &self->v, &__FILE_fd_callbacks, sm) != 0) {
        fd_close(fd);
        return EOF;
    }


    // Make sure that the return value of ftell() issued before the first write
    // lines up with the actual end of file position.
    if ((sm & __kStreamMode_Append) != 0) {
        self->super.cb.seek(self->super.context, 0ll, SEEK_END);
    }

    return 0;
}
