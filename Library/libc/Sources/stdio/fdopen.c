//
//  fdopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdlib.h>
#include <fcntl.h>


FILE *fdopen(int ioc, const char * _Nonnull mode)
{
    __FILE_Mode sm;

    if (__fopen_parse_mode(mode, &sm) == 0) {
        __IOChannel_FILE* self;

        if ((self = malloc(SIZE_OF_FILE_SUBCLASS(__IOChannel_FILE))) == NULL) {
            return NULL;
        }

        if (__fdopen_init(self, ioc, sm | __kStreamMode_FreeOnClose) == 0) {
            int bufmod;
            size_t bufsiz;

            if (fcntl(fileno((FILE*)self), F_GETTYPE) == SEO_FT_TERMINAL) {
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
