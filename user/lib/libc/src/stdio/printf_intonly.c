//
//  printf_intonly.c
//  libc
//
//  Created by Dietmar Planitzer on 2/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


int __v2printf(const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = __vfprintf_i(stdout, format, ap);
    va_end(ap);
    return r;
}
