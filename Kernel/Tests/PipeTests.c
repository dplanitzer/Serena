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
#include <System/System.h>
#include "Asserts.h"


void pipe_test(int argc, char *argv[])
{
    int rioc, wioc;

    assertOK(Pipe_Create(&rioc, &wioc));
    printf("rioc: %d, wioc: %d\n", rioc, wioc);

    const char* pBytesToWrite = "Hello World";
    size_t nBytesToWrite = strlen(pBytesToWrite) + 1;
    ssize_t nBytesWritten = 0;
    assertOK(IOChannel_Write(wioc, pBytesToWrite, nBytesToWrite, &nBytesWritten));
    printf("written: %s, nbytes: %zd\n", pBytesToWrite, nBytesWritten);
    assertEquals(nBytesToWrite, nBytesWritten);

    assertOK(IOChannel_Close(wioc));

    char pBuffer[64];
    ssize_t nBytesRead;
    assertOK(IOChannel_Read(rioc, pBuffer, nBytesWritten, &nBytesRead));
    printf("read: %s, nbytes: %zd\n", pBuffer, nBytesRead);
    assertEquals(nBytesToWrite, nBytesWritten);
    assertEquals(0, strcmp(pBuffer, pBytesToWrite));

    // Should get EOF now since we already closed the write side
    assertOK(IOChannel_Read(rioc, pBuffer, 1, &nBytesRead));
    assertEquals(0, nBytesRead);
    printf("write side is closed, read: nbytes: %zd\n", nBytesRead);

    assertOK(IOChannel_Close(rioc));
    printf("ok\n");
}

////////////////////////////////////////////////////////////////////////////////

static void OnReadFromPipe(void* _Nonnull pValue)
{
    int rioc = (int) pValue;
    ssize_t nBytesRead;
    char buf[16];
    size_t nBytesToRead = sizeof(buf);
    
    while (true) {
        //VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(200));
        buf[0] = '\0';
        assertOK(IOChannel_Read(rioc, buf, nBytesToRead, &nBytesRead));
        buf[nBytesRead] = '\0';

        printf("Reader: '%s' -> %d\n", buf, nBytesRead);
    }
}

static void OnWriteToPipe(void* _Nonnull pValue)
{
    int wioc = (int) pValue;
    const char* bytes = "Hello";
    size_t nBytesToWrite = strlen(bytes);
    ssize_t nBytesWritten;
    
    while (true) {
        clock_wait(TimeInterval_MakeMilliseconds(20));
        assertOK(IOChannel_Write(wioc, bytes, nBytesToWrite, &nBytesWritten));
        
        printf("Writer: '%s'-> %d\n", bytes, nBytesWritten);
    }
}


void pipe2_test(int argc, char *argv[])
{
    int rioc, wioc;
    int utilityQueue;

    assertOK(Pipe_Create(&rioc, &wioc));

    assertOK(DispatchQueue_Create(0, 4, kDispatchQoS_Utility, kDispatchPriority_Normal, &utilityQueue));

    assertOK(DispatchQueue_DispatchAsync(kDispatchQueue_Main, OnWriteToPipe, (void*)wioc));
    assertOK(DispatchQueue_DispatchAsync(utilityQueue, OnReadFromPipe, (void*)rioc));
}
