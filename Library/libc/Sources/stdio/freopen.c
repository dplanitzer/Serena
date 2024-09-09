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
    decl_try_err();
    bool isOldStreamClosed = false;
    __FILE_Mode sm;

    try(__fopen_parse_mode(mode, &sm));

    __fclose(s);
    isOldStreamClosed = true;
    try(__fopen_filename_init((__IOChannel_FILE*)s, filename, sm));
    return s;

catch:
    if (!isOldStreamClosed) {
        __fclose(s);
    }
    errno = err;
    return NULL;
}
