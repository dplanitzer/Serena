//
//  fscanf.c
//  libc
//
//  Created by Dietmar Planitzer on 1/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <ext/__scn.h>
#include "__stdio.h"


int fscanf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vfscanf(s, format, ap);
    va_end(ap);
    return r;
}

int vfscanf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, va_list ap)
{
    scn_t scn;
    int r = EOF;

    __flock(s);
    __fensure_no_err(s);
    __fensure_writeable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_Out);

    __scn_init_fp(&scn, s, (scn_getc_t)__fgetc, (scn_ungetc_t)__ungetc);
    const int res = __scn_scan(&scn, format, ap);
    __scn_deinit(&scn);

    if (res >= 0) {
        r = res;
    }
    else if(scn_failed(&scn)) {
        s->flags.hasError = 1;
    }
    else if (scn_eof(&scn)) {
        s->flags.hasEof = 1;
    }

catch:
    __funlock(s);
    return r;
}
