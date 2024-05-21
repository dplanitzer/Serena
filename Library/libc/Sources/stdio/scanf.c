//
//  scanf.c
//  libc
//
//  Created by Dietmar Planitzer on 1/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>


int scanf(const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfscanf(stdin, format, ap);
    va_end(ap);
    return r;
}

int vscanf(const char *format, va_list ap)
{
    return vfscanf(stdin, format, ap);
}

int fscanf(FILE *s, const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfscanf(s, format, ap);
    va_end(ap);
    return r;
}

int vfscanf(FILE *s, const char *format, va_list ap)
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
