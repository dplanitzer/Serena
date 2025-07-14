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
#include <unistd.h>
#include <sys/timespec.h>
#include "Asserts.h"

static int fds[2];
static dispatch_t gDispatcher;


void pipe_test(int argc, char *argv[])
{
    int fds[2];

    assertOK(pipe(fds));
    printf("rioc: %d, wioc: %d\n", fds[SEO_PIPE_READ], fds[SEO_PIPE_WRITE]);

    const char* pBytesToWrite = "Hello World";
    size_t nBytesToWrite = strlen(pBytesToWrite) + 1;
    ssize_t nBytesWritten = write(fds[SEO_PIPE_WRITE], pBytesToWrite, nBytesToWrite);
    assertGreaterEqual(0, nBytesWritten);
    printf("written: %s, nbytes: %zd\n", pBytesToWrite, nBytesWritten);
    assertEquals(nBytesToWrite, nBytesWritten);

    assertOK(close(fds[SEO_PIPE_WRITE]));

    char pBuffer[64];
    ssize_t nBytesRead = read(fds[SEO_PIPE_READ], pBuffer, nBytesWritten);
    assertGreaterEqual(0, nBytesRead);
    printf("read: %s, nbytes: %zd\n", pBuffer, nBytesRead);
    assertEquals(nBytesToWrite, nBytesWritten);
    assertEquals(0, strcmp(pBuffer, pBytesToWrite));

    // Should get EOF now since we already closed the write side
    nBytesRead = read(fds[SEO_PIPE_READ], pBuffer, 1);
    assertEquals(0, nBytesRead);
    printf("write side is closed, read: nbytes: %zd\n", nBytesRead);

    assertOK(close(fds[SEO_PIPE_READ]));
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
        const ssize_t nBytesRead = read(fd, buf, nBytesToRead);
        assertGreaterEqual(0, nBytesRead);
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
        const ssize_t nBytesWritten = write(fd, bytes, nBytesToWrite);
        assertGreaterEqual(0, nBytesWritten);

        printf("Writer: '%s'-> %d\n", bytes, nBytesWritten);
    }
}


void pipe2_test(int argc, char *argv[])
{
    assertOK(pipe(fds));

    dispatch_attr_t attr = DISPATCH_ATTR_INIT_CONCURRENT_UTILITY(2);
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnWriteToPipe, (void*)fds[SEO_PIPE_WRITE]));
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnReadFromPipe, (void*)fds[SEO_PIPE_READ]));
}
