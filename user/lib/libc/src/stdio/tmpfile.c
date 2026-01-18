//
//  tmpfile.c
//  libc
//
//  Created by Dietmar Planitzer on 2/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include "__stdio.h"


FILE * _Nullable tmpfile(void)
{
    char path[L_tmpnam];
    int fd;

    if (__tmpnam_r(path, &fd) == NULL) {
        return NULL;
    }

    FILE* fp = fdopen(fd, "wb+");
    if (fp == NULL) {
        close(fd);
        return NULL;
    }

    //XXX tmp - replace with kOpen_Private once the kernel supports this option
    unlink(path);

    return fp;
}
