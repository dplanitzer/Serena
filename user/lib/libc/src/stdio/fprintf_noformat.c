//
//  fprintf_noformat.c
//  libc
//
//  Created by Dietmar Planitzer on 2/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>


#ifdef __VBCC__

int __v0fprintf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, ...)
{
    return fputs(format, s);
}

#endif
