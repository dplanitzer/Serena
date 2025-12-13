//
//  sscanf.c
//  libc
//
//  Created by Dietmar Planitzer on 1/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>


int sscanf(const char * _Nonnull _Restrict buffer, const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vsscanf(buffer, format, ap);
    va_end(ap);
    return r;
}

int vsscanf(const char * _Nonnull _Restrict buffer, const char * _Nonnull _Restrict format, va_list ap)
{
    // XXX implement me
    return EOF;
}
