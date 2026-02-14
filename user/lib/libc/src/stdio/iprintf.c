//
//  iprintf.c
//  libc
//
//  Created by Dietmar Planitzer on 2/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


#ifdef __VBCC__
int __v2printf(const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfiprintf(stdout, format, ap);
    va_end(ap);
    return r;
}
#endif

int iprintf(const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfiprintf(stdout, format, ap);
    va_end(ap);
    return r;
}
