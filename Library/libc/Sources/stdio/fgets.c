//
//  fgets.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


char *fgets(char *str, int count, FILE *s)
{
    int nBytesToRead = count - 1;
    int nBytesRead = 0;

    if (count < 1) {
        errno = EINVAL;
        return NULL;
    }

    if (s->flags.hasEof || s->flags.hasError) {
        return NULL;
    }
    if ((s->flags.mode & __kStreamMode_Read) == 0) {
        s->flags.hasError = 1;
        return NULL;
    }
    if (s->flags.orientation == __kStreamOrientation_Wide) {
        s->flags.hasError = 1;
        return NULL;
    }

    while (nBytesToRead-- > 0) {
        const int ch = __fgetc(s);

        if (ch == EOF) {
            break;
        }

        str[nBytesRead++] = ch;

        if (ch == '\n') {
            break;
        }
    }
    str[nBytesRead] = '\0';

    return (s->flags.hasError || (s->flags.hasEof && nBytesRead == 0)) ? NULL : str;
}
