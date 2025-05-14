//
//  stdio.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>
#include <unistd.h>


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
    __init_open_files_lock();
    
    _Stdin = (FILE*)&_StdinObj;
    _Stdout = (FILE*)&_StdoutObj;
    _Stderr = (FILE*)&_StderrObj;

    if (fgetmode(STDIN_FILENO) != 0) {
        __fdopen_init(&_StdinObj, false, STDIN_FILENO, __kStreamMode_Read);
    }
    else {
        __fopen_null_init((FILE*)&_StdinObj, false, __kStreamMode_Read);
    }

    if (fgetmode(STDOUT_FILENO) != 0) {
        __fdopen_init(&_StdoutObj, false, STDOUT_FILENO, __kStreamMode_Write);
    }
    else {
        __fopen_null_init((FILE*)&_StdoutObj, false, __kStreamMode_Write);
    }

    if (fgetmode(STDERR_FILENO) != 0) {
        __fdopen_init(&_StderrObj, false, STDERR_FILENO, __kStreamMode_Write);
    }
    else {
        __fopen_null_init((FILE*)&_StderrObj, false, __kStreamMode_Write);
    }

    atexit(__stdio_exit);
}
