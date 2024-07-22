//
//  snprintf.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <errno.h>
#include "Formatter.h"
#include "Stream.h"


int snprintf(char *buffer, size_t bufsiz, const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vsnprintf(buffer, bufsiz, format, ap);
    va_end(ap);
    return r;
}

int vsnprintf(char *buffer, size_t bufsiz, const char *format, va_list ap)
{
    decl_try_err();
    __Memory_FILE file;
    FILE_Memory mem;
    Formatter fmt;

    if (buffer && bufsiz > 0) {
        mem.base = buffer;
        mem.initialCapacity = bufsiz - 1;
        mem.maximumCapacity = bufsiz - 1;
        mem.initialEof = 0;
        mem.options = 0;

        err = __fopen_memory_init(&file, &mem, "w");
    }
    else {
        // Use a null stream to calculate the length of the formatted string
        err = __fopen_null_init(&file.super, "w");
    }
    if (err != 0) {
        return -err;
    }

    __Formatter_Init(&fmt, &file.super);
    err = __Formatter_vFormat(&fmt, format, ap);
    const size_t nchars = fmt.charactersWritten;
    __Formatter_Deinit(&fmt);
    buffer[fmt.charactersWritten] = '\0';
    __fclose(&file.super);

    return (err == 0) ? nchars : -err;
}
