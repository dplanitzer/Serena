//
//  stdio.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>
#include <System/System.h>

static __IOChannel_FILE _StdinObj;
static __IOChannel_FILE _StdoutObj;
static __IOChannel_FILE _StderrObj;

FILE* _Stdin;
FILE* _Stdout;
FILE* _Stderr;


static void __stdio_exit(void)
{
    fflush(NULL);
    // All open I/O channels will be automatically closed by the kernel when the
    // process terminates.
}


void __stdio_init(void)
{
    _Stdin = (FILE*)&_StdinObj;
    _Stdout = (FILE*)&_StdoutObj;
    _Stderr = (FILE*)&_StderrObj;

    if (IOChannel_GetMode(kIOChannel_Stdin) != 0) {
        __fdopen_init(&_StdinObj, false, kIOChannel_Stdin, __kStreamMode_Read);
    }
    else {
        __fopen_null_init((FILE*)&_StdinObj, __kStreamMode_Read);
    }

    if (IOChannel_GetMode(kIOChannel_Stdout) != 0) {
        __fdopen_init(&_StdoutObj, false, kIOChannel_Stdout, __kStreamMode_Write);
    }
    else {
        __fopen_null_init((FILE*)&_StdoutObj, __kStreamMode_Write);
    }

    if (IOChannel_GetMode(kIOChannel_Stderr) != 0) {
        __fdopen_init(&_StderrObj, false, kIOChannel_Stderr, __kStreamMode_Write);
    }
    else {
        __fopen_null_init((FILE*)&_StderrObj, __kStreamMode_Write);
    }

    atexit(__stdio_exit);
}
