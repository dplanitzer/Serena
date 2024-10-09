//
//  fdreopen.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>


FILE *fdreopen(int ioc, const char *mode, FILE *s)
{
    decl_try_err();
    const bool isFreeOnClose = s->flags.shouldFreeOnClose;
    __FILE_Mode sm;
    
    if ((err = __fopen_parse_mode(mode, &sm)) == EOK) {
        __fclose(s);

        if ((err = __fdopen_init((__IOChannel_FILE*)s, isFreeOnClose, ioc, sm)) == EOK) {
            return s;
        }
    }

    errno = err;
    return NULL;
}
