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
#include "Formatter.h"
#include "Stream.h"


int asprintf(char **str_ptr, const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vasprintf(str_ptr, format, ap);
    va_end(ap);
    return r;
}

int vasprintf(char **str_ptr, const char *format, va_list ap)
{
    const __FILE_Mode sm = __kStreamMode_Write | __kStreamMode_Truncate | __kStreamMode_Create;
    __Memory_FILE file;
    FILE_Memory mem;
    FILE_MemoryQuery mq;
    Formatter fmt;
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

    __Formatter_Init(&fmt, &file.super);
    r = __Formatter_vFormat(&fmt, format, ap);
    putc('\0', &file.super);
    __Formatter_Deinit(&fmt);
    filemem(&file.super, &mq);
    __fclose(&file.super);

    if (str_ptr) {
        if (r >= 0) {
            *str_ptr = mq.base;
        }
        else {
            // We told the stream to not free the memory block. Thus we need to free
            // it if we encountered an error
            free(mq.base);
        }
    }
    
    return r;
}
