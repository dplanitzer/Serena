//
//  scanf.c
//  libc
//
//  Created by Dietmar Planitzer on 1/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>


int scanf(const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfscanf(stdin, format, ap);
    va_end(ap);
    return r;
}

int vscanf(const char * _Nonnull _Restrict format, va_list ap)
{
    return vfscanf(stdin, format, ap);
}
