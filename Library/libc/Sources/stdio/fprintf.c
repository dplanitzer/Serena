//
//  fprintf.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <errno.h>
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

    __Formatter_Init(&fmt, s);
    const errno_t err = __Formatter_vFormat(&fmt, format, ap);
    const size_t nchars = fmt.charactersWritten;
    __Formatter_Deinit(&fmt);
    return (err == 0) ? nchars : -err;
}
