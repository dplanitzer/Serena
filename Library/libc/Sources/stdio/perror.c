//
//  perror.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <__stdio.h>
#include <string.h>


void perror(const char * _Nullable str)
{
    const size_t slen = (str) ? __min(strlen(str), SSIZE_MAX) : 0;
    const char* es = strerror(errno);
    const size_t elen = strlen(es);

    __flock(stdout);
    if (slen > 0) {
        __fwrite(stdout, str, slen);
        __fwrite(stdout, ": ", 2);
    }

    __fwrite(stdout, es, elen);
    __funlock(stdout);
}
