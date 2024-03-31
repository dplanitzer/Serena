//
//  Log.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/17/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <klib/klib.h>
#include <console/Console.h>
#include <console/ConsoleChannel.h>
#include <dispatcher/Lock.h>
#include <driver/DriverManager.h>
#include <User.h>
#include "Formatter.h"

#define PRINT_BUFFER_CAPACITY   80

static Lock         gLock;
static ConsoleRef   gConsole;
static IOChannelRef gConsoleChannel;
static Formatter    gFormatter;
static char    gPrintBuffer[PRINT_BUFFER_CAPACITY];


static errno_t printv_console_sink_locked(FormatterRef _Nonnull self, const char* _Nonnull pBuffer, ssize_t nBytes)
{
    ssize_t nBytesWritten;
    return IOChannel_Write(gConsoleChannel, pBuffer, nBytes, &nBytesWritten);
}

// Initializes the print subsystem.
void print_init(void)
{
    Lock_Init(&gLock);
    Formatter_Init(&gFormatter, printv_console_sink_locked, NULL, gPrintBuffer, PRINT_BUFFER_CAPACITY);
    gConsole = (ConsoleRef) DriverManager_GetDriverForName(gDriverManager, kConsoleName);
    assert(gConsole != NULL);
    try_bang(ConsoleChannel_Create((ObjectRef)gConsole, kOpen_Write, &gConsoleChannel));
}

// Print formatted
void print(const char* _Nonnull format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    printv(format, ap);
    va_end(ap);
}

void printv(const char* _Nonnull format, va_list ap)
{
    Lock_Lock(&gLock);
    Formatter_vFormat(&gFormatter, format, ap);
    Lock_Unlock(&gLock);
}
