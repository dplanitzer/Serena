//
//  setvbuf.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


void setbuf(FILE *s, char *buffer)
{
    if (buffer) {
        (void) setvbuf(s, buffer, _IOFBF, BUFSIZ);
    } else {
        (void) setvbuf(s, NULL, _IONBF, 0);
    }
}

int setvbuf(FILE *s, char *buffer, int mode, size_t size)
{
    // XXX
    return EOF;
}
