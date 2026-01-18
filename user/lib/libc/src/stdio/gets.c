//
//  gets.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


char *_Nullable gets(char * _Nonnull str)
{
    char* r = NULL;
    char* p = str;
    ssize_t res, nBytesRead = 0;
    char ch;

    if (str == NULL) {
        return NULL;
    }

    __flock(stdin);
    __fensure_no_eof_err(stdin);
    __fensure_readable(stdin);
    __fensure_byte_oriented(stdin);
    __fensure_direction(stdin, __kStreamDirection_In);

    while (true) {
        res = __fgetc(&ch, stdin);
        if (res <= 0) {
            break;
        }

        nBytesRead++;
        if (ch == '\n') {
            break;
        }

        *p++ = (char)ch;
    }

    if (nBytesRead > 0) {
        *p = '\0';
        r = str;
    }
    else if (res == 0) {
        stdin->flags.hasEof = 1;
    }
    else {
        stdin->flags.hasError = 1;
    }
    
catch:
    __funlock(stdin);
    return r;
}
