//
//  getline.c
//  libc
//
//  Created by Dietmar Planitzer on 6/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <_math.h>


ssize_t getline(char **line, size_t *n, FILE *s)
{
    return getdelim(line, n, '\n', s);
}

ssize_t getdelim(char **line, size_t *n, int delimiter, FILE *s)
{
    char* buf = *line;
    ssize_t bufSize = __min(*n, __SSIZE_MAX);
    ssize_t i = 0, r;
    char ch;

    __fensure_no_eof_err(s, -1);
    __fensure_readable(s, -1);
    __fensure_byte_oriented(s, -1);
    __fensure_direction(s, __kStreamDirection_In, -1);

    for (;;) {
        r = __fgetc(&ch, s);

        if (r == 1) {
            if ((i == bufSize - 1) || (buf == NULL)) {
                const size_t newBufSize = (bufSize > 0) ? bufSize * 2 : 160;
                char* newBuf = realloc(buf, newBufSize);

                if (newBuf == NULL) {
                    r = -1;
                    break;
                }

                buf = newBuf;
                bufSize = newBufSize;
            }

            buf[i++] = (char)ch;
        }

        if (ch == delimiter || r <= 0) {
            // Stashing the 0 in at the last buffer index is safe because (above)
            // we expand the buffer already when i == bufSize-1
            buf[i] = '\0';
            break;
        }
    }

    if (r < 0 && buf) {
        *buf = '\0';
    }

    *line = buf;
    *n = bufSize;

    
    if (i > 0) {
        return i;
    }
    else if (r == 0) {
        s->flags.hasEof = 1;
        return -1;
    }
    else {
        s->flags.hasError = 1;
        return -1;
    }
}
