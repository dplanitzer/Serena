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
    printf("rioc: %d, wioc: %d\n", fds[PIPE_RD], fds[PIPE_WR]);

    const char* pBytesToWrite = "Hello World";
    size_t nBytesToWrite = strlen(pBytesToWrite) + 1;
    ssize_t nBytesWritten = 0;
    assertOK(write(fds[PIPE_WR], pBytesToWrite, nBytesToWrite, &nBytesWritten));
    printf("written: %s, nbytes: %zd\n", pBytesToWrite, nBytesWritten);
    assertEquals(nBytesToWrite, nBytesWritten);

    assertOK(close(fds[PIPE_WR]));

    char pBuffer[64];
    ssize_t nBytesRead;
    assertOK(read(fds[PIPE_RD], pBuffer, nBytesWritten, &nBytesRead));
    printf("read: %s, nbytes: %zd\n", pBuffer, nBytesRead);
    assertEquals(nBytesToWrite, nBytesWritten);
    assertEquals(0, strcmp(pBuffer, pBytesToWrite));

    // Should get EOF now since we already closed the write side
    assertOK(read(fds[PIPE_RD], pBuffer, 1, &nBytesRead));
    assertEquals(0, nBytesRead);
    printf("write side is closed, read: nbytes: %zd\n", nBytesRead);

    assertOK(close(fds[PIPE_RD]));
    printf("ok\n");
}

////////////////////////////////////////////////////////////////////////////////

static void OnReadFromPipe(int fds[2])
{
    ssize_t nBytesRead;
    char buf[16];
    size_t nBytesToRead = sizeof(buf);
    
    while (true) {
        //VirtualProcessor_Sleep(timespec_from_ms(200));
        buf[0] = '\0';
        assertOK(read(fds[PIPE_RD], buf, nBytesToRead, &nBytesRead));
        buf[nBytesRead] = '\0';

        printf("Reader: '%s' -> %d\n", buf, nBytesRead);
    }
}

static void OnWriteToPipe(int fds[2])
{
    const char* bytes = "Hello";
    size_t nBytesToWrite = strlen(bytes);
    ssize_t nBytesWritten;
    struct timespec dur = timespec_from_ms(20);
    
    while (true) {
        clock_wait(CLOCK_MONOTONIC, &dur);
        assertOK(write(fds[PIPE_WR], bytes, nBytesToWrite, &nBytesWritten));
        
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
