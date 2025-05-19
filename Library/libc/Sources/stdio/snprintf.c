//
//  snprintf.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
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
    const bool hasBuffer = (buffer && bufsiz > 0) ? true : false;
    const __FILE_Mode sm = __kStreamMode_Write | __kStreamMode_Truncate | __kStreamMode_Create;
    __Memory_FILE file;
    FILE_Memory mem;
    Formatter fmt;
    int r;

    if (hasBuffer) {
        buffer[0] = '\0';

        mem.base = buffer;
        mem.initialCapacity = bufsiz - 1;
        mem.maximumCapacity = bufsiz - 1;
        mem.initialEof = 0;
        mem.options = 0;

        r = __fopen_memory_init(&file, false, &mem, sm);
    }
    else {
        // Use a null stream to calculate the length of the formatted string
        r = __fopen_null_init(&file.super, false, sm);
    }
    if (r != 0) {
        return EOF;
    }

    __Formatter_Init(&fmt, &file.super);
    r = __Formatter_vFormat(&fmt, format, ap);
    if (hasBuffer && r >= 0) {
        buffer[r] = '\0';
    }
    __Formatter_Deinit(&fmt);
    __fclose(&file.super);

    if (r < 0) {
        errno = -r;
        if (hasBuffer) {
            buffer[0] = '\0';
        }
    }

    return r;
}
