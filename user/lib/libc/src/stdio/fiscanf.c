//
//  fiscanf.c
//  libc
//
//  Created by Dietmar Planitzer on 3/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


#ifdef __VBCC__
int __v2fscanf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfiscanf(s, format, ap);
    va_end(ap);
    return r;
}
#endif

int fiscanf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfiscanf(s, format, ap);
    va_end(ap);
    return r;
}

int vfiscanf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, va_list ap)
{
    // XXX implement me
    return EOF;
}
