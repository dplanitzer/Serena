//
//  snprintf_intonly.c
//  libc
//
//  Created by Dietmar Planitzer on 2/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ext/__fmt.h>
#include "__stdio.h"


#ifdef __VBCC__

int __v2snprintf(char * _Nullable _Restrict buffer, size_t bufsiz, const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = __vsnprintf_i(buffer, bufsiz, format, ap);
    va_end(ap);
    return r;
}

int __vsnprintf_i(char * _Nullable _Restrict buffer, size_t bufsiz, const char * _Nonnull _Restrict format, va_list ap)
{
    const bool hasBuffer = (buffer && bufsiz > 0) ? true : false;
    const __FILE_Mode sm = __kStreamMode_Write | __kStreamMode_Truncate | __kStreamMode_Create;
    __Memory_FILE file;
    FILE_Memory mem;
    fmt_t fmt;
    int r;

    if (hasBuffer) {
        buffer[0] = '\0';

        mem.base = buffer;
        mem.initialCapacity = bufsiz - 1;
        mem.maximumCapacity = bufsiz - 1;
        mem.initialEof = 0;
        mem.options = 0;

        r = __fopen_memory_init(&file, &mem, sm | __kStreamMode_NoLocking);
    }
    else {
        // Use a null stream to calculate the length of the formatted string
        r = __fopen_null_init(&file.super, sm);
    }
    if (r != 0) {
        return EOF;
    }

    __fmt_init_i(&fmt, &file.super, (fmt_putc_func_t)__fputc, (fmt_write_func_t)__fwrite, true);
    r = __fmt_format(&fmt, format, ap);
    __fmt_deinit(&fmt);
    __fclose(&file.super);

    if (r >= 0) {
        if (hasBuffer) {
            buffer[r] = '\0';
        }
        return r;
    }
    else {
        return EOF;
    }
}

#endif
