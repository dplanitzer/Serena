//
//  getline.c
//  libc
//
//  Created by Dietmar Planitzer on 6/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdio.h>
#include <stdlib.h>
#include <ext/limits.h>
#include <ext/math.h>


ssize_t getline(char **line, size_t * _Nonnull _Restrict n, FILE * _Nonnull _Restrict s)
{
    return getdelim(line, n, '\n', s);
}

ssize_t getdelim(char **line, size_t * _Nonnull _Restrict n, int delimiter, FILE * _Nonnull _Restrict s)
{
    ssize_t r = -1;
    char* buf = *line;
    ssize_t bufSize = __min(*n, SSIZE_MAX);
    ssize_t i = 0, res;
    char ch;

    __flock(s);
    __fensure_no_eof_err(s);
    __fensure_readable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_In);

    for (;;) {
        res = __fgetc(&ch, s);

        if (res == 1) {
            if ((i == bufSize - 1) || (buf == NULL)) {
                const size_t newBufSize = (bufSize > 0) ? bufSize * 2 : 160;
                char* newBuf = realloc(buf, newBufSize);

                if (newBuf == NULL) {
                    res = -1;
                    break;
                }

                buf = newBuf;
                bufSize = newBufSize;
            }

            buf[i++] = (char)ch;
        }

        if (ch == delimiter || res <= 0) {
            // Stashing the 0 in at the last buffer index is safe because (above)
            // we expand the buffer already when i == bufSize-1
            buf[i] = '\0';
            break;
        }
    }

    if (res < 0 && buf) {
        *buf = '\0';
    }

    *line = buf;
    *n = bufSize;

    
    if (i > 0) {
        r = i;
    }
    else if (res == 0) {
        s->flags.hasEof = 1;
    }
    else {
        s->flags.hasError = 1;
    }

catch:
    __funlock(s);
    return r;
}
