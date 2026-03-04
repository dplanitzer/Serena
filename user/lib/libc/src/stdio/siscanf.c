//
//  siscanf.c
//  libc
//
//  Created by Dietmar Planitzer on 3/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/__scn.h>
#include <string.h>
#include "__stdio.h"


#ifdef __VBCC__
int __v2sscanf(char * _Nullable _Restrict buffer, const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vsiscanf(buffer, format, ap);
    va_end(ap);
    return r;
}
#endif

int siscanf(const char * _Nonnull _Restrict buffer, const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vsiscanf(buffer, format, ap);
    va_end(ap);
    return r;
}

int vsiscanf(const char * _Nonnull _Restrict buffer, const char * _Nonnull _Restrict format, va_list ap)
{
    __Memory_FILE file;
    FILE_Memory mem;
    scn_t scn;
    int r;

    mem.base = buffer;
    mem.initialCapacity = strlen(buffer);
    mem.maximumCapacity = mem.initialCapacity;
    mem.initialEof = mem.initialCapacity;
    mem.options = 0;

    r = __fopen_memory_init(&file, &mem, __kStreamMode_Read | __kStreamMode_NoLocking);
    if (r != 0) {
        return EOF;
    }

    __scn_init_i(&scn, &file.super, (scn_getc_t)__fgetc, (scn_ungetc_t)__ungetc);
    r = __scn_scan(&scn, format, ap);
    __scn_deinit(&scn);
    __fclose(&file.super);

    return r;
}
