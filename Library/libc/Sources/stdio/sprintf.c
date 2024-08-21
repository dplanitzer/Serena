//
//  sprintf.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdarg.h>
#include <stdio.h>
#include <limits.h>


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
