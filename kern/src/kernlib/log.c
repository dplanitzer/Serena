//
//  log.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/17/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <kern/log.h>
#include <driver/DriverManager.h>
#include <ext/__fmt.h>
#include <filesystem/IOChannel.h>
#include <kern/cbuf.h>
#include <kpi/fcntl.h>
#include <sched/mtx.h>


enum {
    kSink_RingBuffer,
    kSink_Console
};

#define LOG_BUFFER_SIZE 256

static mtx_t        gLock;
static IOChannelRef gConsoleChannel;
static fmt_t        gFormatter;
static cbuf_t       gRingBuffer;
static char         gLogBuffer[LOG_BUFFER_SIZE];
static int          gCurrentSink;


static ssize_t _lwrite(void* _Nullable _Restrict s, const void * _Restrict buffer, ssize_t nbytes)
{
    ssize_t nBytesWritten;

    switch (gCurrentSink) {
        case kSink_Console:
            IOChannel_Write(gConsoleChannel, buffer, nbytes, &nBytesWritten);
            break;

        default:
            nBytesWritten = cbuf_puts(&gRingBuffer, buffer, nbytes);
            break;
    }

    return nBytesWritten;
}

static ssize_t _lputc(char ch, void* _Nullable s)
{
    return _lwrite(s, &ch, 1);
}


void log_init(void)
{
    mtx_init(&gLock);
    gCurrentSink = kSink_RingBuffer;
    cbuf_init_extbuf(&gRingBuffer, gLogBuffer, LOG_BUFFER_SIZE);
    __fmt_init_i(&gFormatter, NULL, (fmt_putc_func_t)_lputc, (fmt_write_func_t)_lwrite, false);
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
    _lwrite(NULL, buf, nbytes);
    mtx_unlock(&gLock);
}

ssize_t log_read(void* _Nonnull buf, ssize_t nBytesToRead)
{
    ssize_t nbytes;

    mtx_lock(&gLock);
    if (gCurrentSink == kSink_RingBuffer) {
        nbytes = cbuf_gets(&gRingBuffer, buf, nBytesToRead);
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
    __fmt_format(&gFormatter, format, ap);
    mtx_unlock(&gLock);
}
