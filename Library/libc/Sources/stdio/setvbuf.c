//
//  setvbuf.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdlib.h>
#include <ext/limits.h>


int __setvbuf(FILE * _Nonnull _Restrict s, char * _Nullable _Restrict buffer, int mode, size_t size)
{
    switch (mode) {
        case _IOFBF:
        case _IOLBF:
        case _IONBF:
            break;

        default:
            errno = EINVAL;
            return EOF;
    }

    if (size > SSIZE_MAX || (mode > _IONBF && size < 1)) {
        errno = EINVAL;
        return EOF;
    }

    
    if (s->flags.bufferOwned) {
        free(s->buffer);
    }
    s->buffer = NULL;
    s->bufferCapacity = 0;
    s->bufferCount = 0;
    s->bufferIndex = -1;
    s->flags.bufferOwned = 0;
    s->flags.bufferMode = _IONBF;


    if (mode == _IOLBF || mode == _IOFBF) {
        if (buffer == NULL) {
            buffer = malloc(size);
            if (buffer == NULL) {
                return EOF;
            }

            s->flags.bufferOwned = 1;
        }
        else {
            s->flags.bufferOwned = 0;
        }
        s->buffer = buffer;
        s->bufferCapacity = size;
        s->flags.bufferMode = mode;
    }

    return 0;
}

int setvbuf(FILE * _Nonnull _Restrict s, char * _Nullable _Restrict buffer, int mode, size_t size)
{
    return __setvbuf(s, buffer, mode, size);
}

void setbuf(FILE * _Nonnull _Restrict s, char * _Nullable _Restrict buffer)
{
    if (buffer) {
        (void) setvbuf(s, buffer, _IOFBF, BUFSIZ);
    } else {
        (void) setvbuf(s, NULL, _IONBF, 0);
    }
}
