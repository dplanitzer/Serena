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
#include <ext/__fmt.h>
#include "Stream.h"


int fprintf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfprintf(s, format, ap);
    va_end(ap);

    return r;
}

int vfprintf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, va_list ap)
{
    fmt_t fmt;

    __fensure_no_err(s, EOF);
    __fensure_writeable(s, EOF);
    __fensure_byte_oriented(s, EOF);
    __fensure_direction(s, __kStreamDirection_Out, EOF);

    __fmt_init(&fmt, s, (fmt_putc_func_t)__fputc, (fmt_write_func_t)__fwrite, false);
    const int r = __fmt_format(&fmt, format, ap);
    __fmt_deinit(&fmt);

    if (r >= 0) {
        return r;
    }
    else {
        s->flags.hasError = 1;
        return EOF;
    }
}
