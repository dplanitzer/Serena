//
//  Log.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/17/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Log.h"
#include <dispatcher/Lock.h>
#include <Catalog.h>
#include <filesystem/IOChannel.h>
#include <klib/RingBuffer.h>
#include "Formatter.h"


enum {
    kSink_RingBuffer,
    kSink_Console
};

#define LOG_BUFFER_SIZE 256

static Lock             gLock;
static IOChannelRef     gConsoleChannel;
static struct Formatter gFormatter;
static RingBuffer       gRingBuffer;
static char             gLogBuffer[LOG_BUFFER_SIZE];
static int              gCurrentSink;


static void _log_sink(struct Formatter* _Nonnull self, const char* _Nonnull buf, ssize_t nbytes)
{
    switch (gCurrentSink) {
        case kSink_Console: {
            ssize_t nBytesWritten;
            IOChannel_Write(gConsoleChannel, buf, nbytes, &nBytesWritten);
            break;
        }

        default:
            RingBuffer_PutBytes(&gRingBuffer, buf, nbytes);
            break;
    }
}


void log_init(void)
{
    Lock_Init(&gLock);
    gCurrentSink = kSink_RingBuffer;
    RingBuffer_InitWithBuffer(&gRingBuffer, gLogBuffer, LOG_BUFFER_SIZE);
    Formatter_Init(&gFormatter, _log_sink, NULL);
}

static errno_t log_open_console(void)
{
    decl_try_err();

    if (gDriverCatalog) {
        err = Catalog_Open(gDriverCatalog, "/console", kOpen_Write, &gConsoleChannel);
    }
    else {
        err = ENODEV;
    }
    return err;
}

bool log_switch_to_console(void)
{
    decl_try_err();
    bool r = false;

    Lock_Lock(&gLock);

    if (gCurrentSink != kSink_Console) {
        err = log_open_console();
        if (err == EOK) {
            gCurrentSink = kSink_Console;
            r = true;
        }
    }

    Lock_Unlock(&gLock);
    return r;
}


void log_write(const char* _Nonnull buf, ssize_t nbytes)
{
    Lock_Lock(&gLock);
    _log_sink(&gFormatter, buf, nbytes);
    Lock_Unlock(&gLock);
}

ssize_t log_read(void* _Nonnull buf, ssize_t nBytesToRead)
{
    ssize_t nbytes;

    Lock_Lock(&gLock);
    if (gCurrentSink == kSink_RingBuffer) {
        nbytes = RingBuffer_GetBytes(&gRingBuffer, buf, nBytesToRead);
    }
    else {
        nbytes = 0;
    }
    Lock_Unlock(&gLock);
    return nbytes;
}

void printf(const char* _Nonnull format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

void vprintf(const char* _Nonnull format, va_list ap)
{
    Lock_Lock(&gLock);
    Formatter_vFormat(&gFormatter, format, ap);
    Lock_Unlock(&gLock);
}
