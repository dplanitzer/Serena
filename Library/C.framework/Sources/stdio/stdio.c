//
//  stdio.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <string.h>
#include <apollo/apollo.h>


void __stdio_init(void)
{
    // XXX temporary until we'll put something like an ini process in place
    int fd0, fd1;
    //    assert(open("/dev/console", O_RDONLY, &fd0) == 0);
    //    assert(open("/dev/console", O_WRONLY, &fd1) == 0);
    open("/dev/console", O_RDONLY, &fd0);
    open("/dev/console", O_WRONLY, &fd1);
    // XXX temporary until we'll put something like an ini process in place

    __fdopen_init(stdin, STDIN_FILENO, "r");
    __fdopen_init(stdout, STDOUT_FILENO, "w");
    // XXX add support for stderr
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
    putchar('\n');
}

int remove(const char* path)
{
    decl_try_err();

    err = unlink(path);
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

    err = sys_rename(oldpath, newpath);
    if (err != 0) {
        errno = err;
        return -1;
    }
    else {
        return 0;
    }
}
