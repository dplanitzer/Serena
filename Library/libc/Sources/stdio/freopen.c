//
//  freopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *freopen(const char *filename, const char *mode, FILE *s)
{
    if ((__fopen_parse_mode(mode) & (__kStreamMode_Read|__kStreamMode_Write)) == 0) {
        fclose(s);
        return NULL;
    }

    __fclose(s);
    const errno_t err = __fopen_filename_init((__IOChannel_FILE*)s, filename, mode);
    if (err != 0) {
        errno = err;
        return NULL;
    }
    else {
        return s;
    }
}
