//
//  Log.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/17/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Log.h"
#include "Formatter.h"
#include <driver/DriverManager.h>
#include <filesystem/IOChannel.h>
#include <klib/RingBuffer.h>
#include <sched/mtx.h>
#include <kpi/fcntl.h>


enum {
    kSink_RingBuffer,
    kSink_Console
};

#define LOG_BUFFER_SIZE 256

static mtx_t            gLock;
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
    mtx_init(&gLock);
    gCurrentSink = kSink_RingBuffer;
    RingBuffer_InitWithBuffer(&gRingBuffer, gLogBuffer, LOG_BUFFER_SIZE);
    Formatter_Init(&gFormatter, _log_sink, NULL);
}

static errno_t log_open_console(void)
{
    decl_try_err();

    if (gDriverManager) {
        err = DriverManager_Open(gDriverManager, "/console", O_WRONLY, &gConsoleChannel);
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

    mtx_lock(&gLock);

    if (gCurrentSink != kSink_Console) {
        err = log_open_console();
        if (err == EOK) {
            gCurrentSink = kSink_Console;
            r = true;
        }
    }
    else {
        r = true;
    }

    mtx_unlock(&gLock);
    return r;
}


void log_write(const char* _Nonnull buf, ssize_t nbytes)
{
    mtx_lock(&gLock);
    _log_sink(&gFormatter, buf, nbytes);
    mtx_unlock(&gLock);
}

ssize_t log_read(void* _Nonnull buf, ssize_t nBytesToRead)
{
    ssize_t nbytes;

    mtx_lock(&gLock);
    if (gCurrentSink == kSink_RingBuffer) {
        nbytes = RingBuffer_GetBytes(&gRingBuffer, buf, nBytesToRead);
    }
    else {
        nbytes = 0;
    }
    mtx_unlock(&gLock);
    return nbytes;
}

const char* _Nonnull log_buffer(void)
{
    return gLogBuffer;
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
    mtx_lock(&gLock);
    Formatter_vFormat(&gFormatter, format, ap);
    mtx_unlock(&gLock);
}
