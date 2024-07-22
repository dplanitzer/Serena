//
//  printf.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include "Formatter.h"
#include "Stream.h"


int printf(const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfprintf(stdout, format, ap);
    va_end(ap);
    return r;
}

int vprintf(const char *format, va_list ap)
{
    return vfprintf(stdout, format, ap);
}
