//
//  scanf.c
//  Apollo
//
//  Created by Dietmar Planitzer on 1/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>


int scanf(const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vscanf(format, ap);
    va_end(ap);
    return r;
}

int vscanf(const char *format, va_list ap)
{
    // XXX implement me
    return EOF;
}

int sscanf(const char *buffer, const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vsscanf(buffer, format, ap);
    va_end(ap);
    return r;
}

int vsscanf(const char *buffer, const char *format, va_list ap)
{
    // XXX implement me
    return EOF;
}
