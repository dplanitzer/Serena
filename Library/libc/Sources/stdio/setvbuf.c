//
//  setvbuf.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


// Expects:
// - 's' is not NULL
int __setvbuf(FILE * _Restrict s, char * _Restrict buffer, int mode, size_t size)
{
    switch (mode) {
        case _IOFBF:
        case _IOLBF:
            if (buffer) {
                s->buffer = buffer;
                s->bufferCapacity = size;
                s->bufferCount = 0;
                s->bufferIndex = -1;
                s->flags.bufferOwned = 0;
            }
            else {
                char* p = malloc(size);
                if (p == NULL) {
                    return EOF;
                }

                s->buffer = p;
                s->bufferCapacity = size;
                s->bufferCount = 0;
                s->bufferIndex = -1;
                s->flags.bufferOwned = 1;
            }
            s->flags.bufferMode = mode;
            return 0;

        case _IONBF:
            if (s->flags.bufferOwned) {
                free(s->buffer);
            }
            s->buffer = NULL;
            s->bufferCapacity = 0;
            s->bufferCount = 0;
            s->bufferIndex = -1;
            s->flags.bufferOwned = 0;
            s->flags.bufferMode = _IONBF;
            return 0;

        default:
            errno = EINVAL;
            return EOF;
    }
}

int setvbuf(FILE * _Restrict s, char * _Restrict buffer, int mode, size_t size)
{
    return __setvbuf(s, buffer, mode, size);
}

void setbuf(FILE * _Restrict s, char * _Restrict buffer)
{
    if (buffer) {
        (void) setvbuf(s, buffer, _IOFBF, BUFSIZ);
    } else {
        (void) setvbuf(s, NULL, _IONBF, 0);
    }
}
