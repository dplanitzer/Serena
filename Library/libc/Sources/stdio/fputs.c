//
//  fputs.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <limits.h>


int fputs(const char *str, FILE *s)
{
    int nCharsWritten = 0;

    while (*str != '\0') {
        const int r = fputc(*str++, s);

        if (r == EOF) {
            return EOF;
        }

        if (nCharsWritten < INT_MAX) {
            nCharsWritten++;
        }
    }
    return nCharsWritten;
}
