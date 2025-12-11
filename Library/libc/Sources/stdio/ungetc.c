//
//  ungetc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


int ungetc(int ch, FILE *s)
{
    __fensure_no_eof_err(s, EOF);
    __fensure_readable(s, EOF);
    __fensure_byte_oriented(s, EOF);
    __fensure_direction(s, __kStreamDirection_In, EOF);
    
    // XXX
    return EOF;
}
