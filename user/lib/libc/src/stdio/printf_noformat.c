//
//  printf_noformat.c
//  libc
//
//  Created by Dietmar Planitzer on 2/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>


// VBCC only
int __v0printf(const char * _Nonnull _Restrict format, ...)
{
    return fputs(format, stdout);
}
