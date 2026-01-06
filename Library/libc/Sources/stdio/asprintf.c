//
//  asprintf.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ext/__fmt.h>
#include "__stdio.h"


int asprintf(char **str_ptr, const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vasprintf(str_ptr, format, ap);
    va_end(ap);
    return r;
}

int vasprintf(char **str_ptr, const char * _Nonnull _Restrict format, va_list ap)
{
    const __FILE_Mode sm = __kStreamMode_Write | __kStreamMode_Truncate | __kStreamMode_Create;
    __Memory_FILE file;
    FILE_Memory mem;
    FILE_MemoryQuery mq;
    fmt_t fmt;
    int r = 0;

    if (str_ptr) {
        *str_ptr = NULL;
    }

    if (str_ptr) {
        *str_ptr = NULL;
        mem.base = NULL;
        mem.initialCapacity = 128;
        mem.maximumCapacity = SIZE_MAX;
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

    __fmt_init(&fmt, &file.super, (fmt_putc_func_t)__fputc, (fmt_write_func_t)__fwrite, false);
    const int r1 = __fmt_format(&fmt, format, ap);
    const int r2 = __fputc('\0', &file.super);
    __fmt_deinit(&fmt);
    __filemem(&file.super, &mq);
    __fclose(&file.super);

    if (r1 >= 0 && r2 >= 0) {
        if (str_ptr) {
            *str_ptr = mq.base;
        }
        return r1;
    }
    else {
        // We told the stream to not free the memory block. Thus we need to free
        // it if we encountered an error
        if (str_ptr) {
            free(mq.base);
        }
        return EOF;
    }
}
