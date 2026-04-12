//
//  fdopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdlib.h>
#include <serena/fd.h>

FILE *fdopen(int fd, const char * _Nonnull mode)
{
    const int ftype = fd_type(fd);
    __FILE_Mode sm;

    if (ftype > FD_TYPE_INVALID && __fopen_parse_mode(mode, &sm) == 0) {
        __IOChannel_FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(__IOChannel_FILE))) == NULL) {
            return NULL;
        }

        if (__fdopen_init(self, fd, sm | __kStreamMode_FreeOnClose) == 0) {
            int bufmod;
            size_t bufsiz;

            if (ftype == FD_TYPE_TERMINAL) {
                bufmod = _IOLBF;
                bufsiz = 256;
            }
            else {
                bufmod = _IOFBF;
                bufsiz = BUFSIZ;
            }

            if (__setvbuf((FILE*)self, NULL, bufmod, bufsiz) == EOF) {
                // ignore memory alloc failure - just return an unbuffered stream
                errno = 0;
            }

            __freg_file((FILE*)self);
            return (FILE*)self;
        }

        free(self);
    }

    return NULL;
}
