//
//  freopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


FILE *freopen(const char * _Nonnull _Restrict filename, const char * _Nonnull _Restrict mode, FILE * _Nonnull _Restrict s)
{
    __FILE_Mode sm;
    int r = EOF;

    if (__fopen_parse_mode(mode, &sm) == 0) {
        __flock(s);
        __fclose(s);

        r = __fopen_filename_init((__IOChannel_FILE*)s, filename, sm | __kStreamMode_Reinit);
        __funlock(s);
    }
    
    if (r == 0) {
        return s;
    }
    else {
        fclose(s);
        return NULL;
    }
}
