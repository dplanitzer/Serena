//
//  printf.c
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


int printf(const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vprintf(format, ap);
    va_end(ap);
    return r;
}

int vprintf(const char *format, va_list ap)
{
    Formatter fmt;

    __Formatter_Init(&fmt, stdout);
    const errno_t err = __Formatter_vFormat(&fmt, format, ap);
    const size_t nchars = fmt.charactersWritten;
    __Formatter_Deinit(&fmt);
    return (err == 0) ? nchars : -err;
}


////////////////////////////////////////////////////////////////////////////////

int sprintf(char *buffer, const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vsprintf(buffer, format, ap);
    va_end(ap);
    return r;
}

int vsprintf(char *buffer, const char *format, va_list ap)
{
    return vsnprintf(buffer, SIZE_MAX, format, ap);
}

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
        mem.freeOnClose = false;

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


////////////////////////////////////////////////////////////////////////////////

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
    decl_try_err();
    __Memory_FILE file;
    FILE_Memory mem;
    FILE_MemoryQuery mq;
    Formatter fmt;

    if (str_ptr) {
        *str_ptr = NULL;
        mem.base = NULL;
        mem.initialCapacity = 128;
        mem.maximumCapacity = SIZE_MAX;
        mem.initialEof = 0;
        mem.freeOnClose = false;

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
    const int r = (err == 0) ? fputc('\0', &file.super) : EOF; // write terminating NUL
    __Formatter_Deinit(&fmt);
    filemem(&file.super, &mq);
    __fclose(&file.super);

    if (r != EOF) {
        if (str_ptr) *str_ptr = mq.base;
        return nchars;
     } else {
        if (str_ptr) {
            // we told the stream to not free the memory block, however we may have
            // gotten a partially filled block. So free it manually.
            free(mq.base);
            *str_ptr = NULL;
        }
        return (err != 0) ? -err : -errno;
     }
}
