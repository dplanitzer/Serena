//
//  fscanf.c
//  libc
//
//  Created by Dietmar Planitzer on 1/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>


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
