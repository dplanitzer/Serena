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

    while (nBytesToRead-- > 0) {
        const int ch = fgetc(s);

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
