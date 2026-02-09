//
//  sprintf_intonly.c
//  libc
//
//  Created by Dietmar Planitzer on 2/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <stdarg.h>
#include "__stdio.h"
#include <limits.h>


// VBCC only
int __v2sprintf(char * _Nullable _Restrict buffer, const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = __vsnprintf_i(buffer, SIZE_MAX, format, ap);
    va_end(ap);
    return r;
}
