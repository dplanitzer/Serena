//
//  tmpfile.c
//  libc
//
//  Created by Dietmar Planitzer on 2/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <System/File.h>

extern char *__tmpnam_r(char *filename, int* pOutIoc);


FILE *tmpfile(void)
{
    char path[L_tmpnam];
    int ioc;

    if (__tmpnam_r(path, &ioc) == NULL) {
        return NULL;
    }

    FILE* fp = fdopen(ioc, "wb+");
    if (fp == NULL) {
        IOChannel_Close(ioc);
        return NULL;
    }

    //XXX tmp - replace with kOpen_Private once the kernel supports this option
    File_Unlink(path);

    return fp;
}
