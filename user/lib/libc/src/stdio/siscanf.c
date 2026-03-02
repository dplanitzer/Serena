//
//  siscanf.c
//  libc
//
//  Created by Dietmar Planitzer on 3/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

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
    // XXX implement me
    return EOF;
}
