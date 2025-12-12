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
#include "Stream.h"


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

    __fensure_no_err(s, EOF);
    __fensure_writeable(s, EOF);
    __fensure_byte_oriented(s, EOF);
    __fensure_direction(s, __kStreamDirection_Out, EOF);

    __Formatter_Init(&fmt, s, false);
    const int r = __Formatter_vFormat(&fmt, format, ap);
    __Formatter_Deinit(&fmt);

    if (r >= 0) {
        return r;
    }
    else {
        s->flags.hasError = 1;
        return EOF;
    }
}
