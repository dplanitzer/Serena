//
//  Log.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/17/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <klib/klib.h>
#include <console/Console.h>
#include "DriverManager.h"
#include "Formatter.h"
#include "Lock.h"

#define PRINT_BUFFER_CAPACITY   80

static Lock         gLock;
static ConsoleRef   gConsole;
static IOChannelRef gConsoleChannel;
static Formatter    gFormatter;
static Character    gPrintBuffer[PRINT_BUFFER_CAPACITY];


static ErrorCode printv_console_sink_locked(FormatterRef _Nonnull self, const Character* _Nonnull pBuffer, ByteCount nBytes)
{
    ByteCount nBytesWritten;
    return IOChannel_Write(gConsoleChannel, pBuffer, nBytes, &nBytesWritten);
}

// Initializes the print subsystem.
void print_init(void)
{
    User user = {kRootUserId, kRootGroupId}; //XXX
    Lock_Init(&gLock);
    Formatter_Init(&gFormatter, printv_console_sink_locked, NULL, gPrintBuffer, PRINT_BUFFER_CAPACITY);
    gConsole = (ConsoleRef) DriverManager_GetDriverForName(gDriverManager, kConsoleName);
    assert(gConsole != NULL);
    try_bang(IOResource_Open(gConsole, NULL/*XXX*/, O_WRONLY, user, &gConsoleChannel));
}

// Print formatted
void print(const Character* _Nonnull format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    printv(format, ap);
    va_end(ap);
}

void printv(const Character* _Nonnull format, va_list ap)
{
    Lock_Lock(&gLock);
    Formatter_vFormat(&gFormatter, format, ap);
    Lock_Unlock(&gLock);
}
