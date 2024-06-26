//
//  stdio.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <string.h>
#include <System/System.h>

static __IOChannel_FILE _StdinObj;
static __IOChannel_FILE _StdoutObj;
static __IOChannel_FILE _StderrObj;


void __stdio_init(void)
{
    // XXX temporary until we'll put something like an init process in place
    int fd0, fd1, fd2;
    //    assert(File_Open("/dev/console", kOpen_Read, &fd0) == 0);
    //    assert(File_Open("/dev/console", kOpen_Write, &fd1) == 0);
    File_Open("/dev/console", kOpen_Read, &fd0);
    File_Open("/dev/console", kOpen_Write, &fd1);
    File_Open("/dev/console", kOpen_Write, &fd2);
    // XXX temporary until we'll put something like an init process in place

    _Stdin = (FILE*)&_StdinObj;
    _Stdout = (FILE*)&_StdoutObj;
    _Stderr = (FILE*)&_StderrObj;

    if (IOChannel_GetMode(kIOChannel_Stdin) != 0) {
        __fdopen_init(&_StdinObj, false, kIOChannel_Stdin, "r");
    }
    else {
        __fopen_null_init((FILE*)&_StdinObj, "r");
    }

    if (IOChannel_GetMode(kIOChannel_Stdout) != 0) {
        __fdopen_init(&_StdoutObj, false, kIOChannel_Stdout, "w");
    }
    else {
        __fopen_null_init((FILE*)&_StdoutObj, "w");
    }

    if (IOChannel_GetMode(kIOChannel_Stderr) != 0) {
        __fdopen_init(&_StderrObj, false, kIOChannel_Stderr, "w");
    }
    else {
        __fopen_null_init((FILE*)&_StderrObj, "w");
    }
}

void __stdio_exit(void)
{
    fflush(NULL);
    // All open I/O channels will be automatically closed by the kernel when the
    // process terminates.
}

void perror(const char *str)
{
    if (str && *str != '\0') {
        puts(str);
        puts(": ");
    }

    puts(strerror(errno));
}

int remove(const char* path)
{
    decl_try_err();

    err = File_Unlink(path);
    if (err != 0) {
        errno = err;
        return -1;
    }
    else {
        return 0;
    }
}

int rename(const char* oldpath, const char* newpath)
{
    decl_try_err();

    err = File_Rename(oldpath, newpath);
    if (err != 0) {
        errno = err;
        return -1;
    }
    else {
        return 0;
    }
}
