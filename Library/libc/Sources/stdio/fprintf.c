//
//  fprintf.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include "Formatter.h"


int fprintf(FILE *s, const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfprintf(s, format, ap);
    va_end(ap);

    return r;
}

int vfprintf(FILE *s, const char *format, va_list ap)
{
    Formatter fmt;
    int r;

    __Formatter_Init(&fmt, s);
    const errno_t err = __Formatter_vFormat(&fmt, format, ap);
    if (err == EOK) {
        r = (fmt.charactersWritten > INT_MAX) ? INT_MAX : (int)fmt.charactersWritten;
    }
    else {
        errno = err;
        r = -err;
    }
    __Formatter_Deinit(&fmt);

    return r;
}
