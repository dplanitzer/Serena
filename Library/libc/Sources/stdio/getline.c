//
//  getline.c
//  libc
//
//  Created by Dietmar Planitzer on 6/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <kern/_math.h>


ssize_t getline(char **line, size_t *n, FILE *s)
{
    return getdelim(line, n, '\n', s);
}

ssize_t getdelim(char **line, size_t *n, int delimiter, FILE *s)
{
    char* buf = *line;
    ssize_t bufSize = __min(*n, __SSIZE_MAX);
    ssize_t i = 0;

    for (;;) {
        const int ch = fgetc(s);

        if (ch != EOF) {
            if ((i == bufSize - 1) || (buf == NULL)) {
                const size_t newBufSize = (bufSize > 0) ? bufSize * 2 : 160;
                char* newBuf = realloc(buf, newBufSize);

                if (newBuf == NULL) {
                    i = -1;
                    break;
                }

                buf = newBuf;
                bufSize = newBufSize;
            }

            buf[i++] = (char)ch;
        }

        if (ch == delimiter || ch == EOF) {
            // Stashing the 0 in at the last buffer index is safe because (above)
            // we expand the buffer already when i == bufSize-1
            buf[i] = '\0';
            break;
        }
    }

    if (i == -1 && buf) {
        *buf = '\0';
    }

    *line = buf;
    *n = bufSize;

    return i;
}
