//
//  PipeTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 2/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/dispatch.h>
#include <sys/timespec.h>
#include "Asserts.h"


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

static void OnReadFromPipe(int fds[2])
{
    char buf[16];
    size_t nBytesToRead = sizeof(buf);
    
    while (true) {
        //VirtualProcessor_Sleep(timespec_from_ms(200));
        buf[0] = '\0';
        const ssize_t nBytesRead = read(fds[SEO_PIPE_READ], buf, nBytesToRead);
        assertGreaterEqual(0, nBytesRead);
        buf[nBytesRead] = '\0';

        printf("Reader: '%s' -> %d\n", buf, nBytesRead);
    }
}

static void OnWriteToPipe(int fds[2])
{
    const char* bytes = "Hello";
    size_t nBytesToWrite = strlen(bytes);
    struct timespec dur = timespec_from_ms(20);
    
    while (true) {
        clock_wait(CLOCK_MONOTONIC, &dur);
        const ssize_t nBytesWritten = write(fds[SEO_PIPE_WRITE], bytes, nBytesToWrite);
        assertGreaterEqual(0, nBytesWritten);

        printf("Writer: '%s'-> %d\n", bytes, nBytesWritten);
    }
}


void pipe2_test(int argc, char *argv[])
{
    int fds[2];

    assertOK(pipe(fds));

    const int utilityQueue = dispatch_create(0, 4, kDispatchQoS_Utility, kDispatchPriority_Normal);
    assertGreaterEqual(0, utilityQueue);
    assertOK(dispatch_async(kDispatchQueue_Main, (dispatch_func_t)OnWriteToPipe, fds));
    assertOK(dispatch_async(utilityQueue, (dispatch_func_t)OnReadFromPipe, fds));
}
