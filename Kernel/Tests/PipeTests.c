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
