//
//  iscanf.c
//  libc
//
//  Created by Dietmar Planitzer on 3/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


#ifdef __VBCC__
int __v2scanf(const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfiscanf(stdin, format, ap);
    va_end(ap);
    return r;
}
#endif

int scanf(const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfiscanf(stdin, format, ap);
    va_end(ap);
    return r;
}

int viscanf(const char * _Nonnull _Restrict format, va_list ap)
{
    return vfiscanf(stdin, format, ap);
}
