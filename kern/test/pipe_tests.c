//
//  pipe_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 2/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <dispatch.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ext/timespec.h>
#include <serena/clock.h>
#include <serena/pipe.h>
#include <serena/process.h>
#include "Asserts.h"

static int fds[2];
static dispatch_t gDispatcher;


void pipe_test(int argc, char *argv[])
{
    int fds[2];

    assert_ok(pipe_create(fds));
    printf("rioc: %d, wioc: %d\n", fds[PIPE_FD_READ], fds[PIPE_FD_WRITE]);

    const char* pBytesToWrite = "Hello World";
    size_t nBytesToWrite = strlen(pBytesToWrite) + 1;
    ssize_t nBytesWritten = fd_write(fds[PIPE_FD_WRITE], pBytesToWrite, nBytesToWrite);
    assert_ssize_ge(0, nBytesWritten);
    printf("written: %s, nbytes: %zd\n", pBytesToWrite, nBytesWritten);
    assert_ssize_eq(nBytesToWrite, nBytesWritten);

    assert_ok(fd_close(fds[PIPE_FD_WRITE]));

    char pBuffer[64];
    ssize_t nBytesRead = fd_read(fds[PIPE_FD_READ], pBuffer, nBytesWritten);
    assert_ssize_ge(0, nBytesRead);
    printf("read: %s, nbytes: %zd\n", pBuffer, nBytesRead);
    assert_ssize_eq(nBytesToWrite, nBytesWritten);
    assert_int_eq(0, strcmp(pBuffer, pBytesToWrite));

    // Should get EOF now since we already closed the write side
    nBytesRead = fd_read(fds[PIPE_FD_READ], pBuffer, 1);
    assert_ssize_eq(0, nBytesRead);
    printf("write side is closed, read: nbytes: %zd\n", nBytesRead);

    assert_ok(fd_close(fds[PIPE_FD_READ]));
    printf("ok\n");
}

////////////////////////////////////////////////////////////////////////////////

static void OnReadFromPipe(int fd)
{
    char buf[16];
    size_t nBytesToRead = sizeof(buf);
    struct timespec dly;
    
    timespec_from_ms(&dly, 200);

    while (true) {
        //clock_nanosleep(CLOCK_MONOTONIC, 0, &dly, NULL);
        buf[0] = '\0';
        const ssize_t nBytesRead = fd_read(fd, buf, nBytesToRead);
        assert_ssize_ge(0, nBytesRead);
        buf[nBytesRead] = '\0';

        printf("Reader: '%s' -> %d\n", buf, nBytesRead);
    }
}

static void OnWriteToPipe(int fd)
{
    const char* bytes = "Hello";
    size_t nBytesToWrite = strlen(bytes);
    struct timespec dur;
    
    timespec_from_ms(&dur, 20);
    
    while (true) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &dur, NULL);
        const ssize_t nBytesWritten = fd_write(fd, bytes, nBytesToWrite);
        assert_ssize_ge(0, nBytesWritten);

        printf("Writer: '%s'-> %d\n", bytes, nBytesWritten);
    }
}


void pipe2_test(int argc, char *argv[])
{
    assert_ok(pipe_create(fds));

    dispatch_attr_t attr = DISPATCH_ATTR_INIT_FIXED_CONCURRENT_UTILITY(2);
    gDispatcher = dispatch_create(&attr);
    assert_not_null(gDispatcher);

    assert_ok(dispatch_async(gDispatcher, (dispatch_async_func_t)OnWriteToPipe, (void*)fds[PIPE_FD_WRITE]));
    assert_ok(dispatch_async(gDispatcher, (dispatch_async_func_t)OnReadFromPipe, (void*)fds[PIPE_FD_READ]));
}
