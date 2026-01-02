//
//  __stdio_init.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <fcntl.h>
#include <stdlib.h>


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
    int bufmod;
    size_t bufsiz;

    __init_open_files_lock();
    
    _Stdin = (FILE*)&_StdinObj;
    _Stdout = (FILE*)&_StdoutObj;
    _Stderr = (FILE*)&_StderrObj;

    if (fcntl(STDIN_FILENO, F_GETFL) != -1) {
        __fdopen_init(&_StdinObj, false, STDIN_FILENO, __kStreamMode_Read);

        if (fcntl(STDIN_FILENO, F_GETTYPE) == SEO_FT_TERMINAL) {
            bufmod = _IOLBF;
            bufsiz = 256;
        }
        else {
            bufmod = _IOFBF;
            bufsiz = BUFSIZ;
        }

        __setvbuf(_Stdin, NULL, bufmod, bufsiz);
    }
    else {
        __fopen_null_init(_Stdin, false, __kStreamMode_Read);
    }

    if (fcntl(STDOUT_FILENO, F_GETFL) != -1) {
        __fdopen_init(&_StdoutObj, false, STDOUT_FILENO, __kStreamMode_Write);

        if (fcntl(STDOUT_FILENO, F_GETTYPE) == SEO_FT_TERMINAL) {
            bufmod = _IOLBF;
            bufsiz = 256;
        }
        else {
            bufmod = _IOFBF;
            bufsiz = BUFSIZ;
        }

        __setvbuf(_Stdout, NULL, bufmod, bufsiz);
    }
    else {
        __fopen_null_init(_Stdout, false, __kStreamMode_Write);
    }

    if (fcntl(STDERR_FILENO, F_GETFL) != -1) {
        __fdopen_init(&_StderrObj, false, STDERR_FILENO, __kStreamMode_Write);
    }
    else {
        __fopen_null_init(_Stderr, false, __kStreamMode_Write);
    }

    atexit(__stdio_exit);
}
