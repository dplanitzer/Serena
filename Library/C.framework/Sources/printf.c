//
//  printf.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <syscall.h>
#include "Formatter.h"


int printf(const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vprintf(format, ap);
    va_end(ap);
    return r;
}

static errno_t vprintf_console_sink(FormatterRef _Nullable self, const char* _Nonnull pBuffer, size_t nBytes)
{
    return __write(pBuffer, nBytes);
}

int vprintf(const char *format, va_list ap)
{
    Formatter fmt;

    __Formatter_Init(&fmt, vprintf_console_sink, NULL);
    const errno_t err = __Formatter_vFormat(&fmt, format, ap);
    const size_t nchars = fmt.charactersWritten;
    __Formatter_Deinit(&fmt);
    return (err == 0) ? nchars : -err;
}


////////////////////////////////////////////////////////////////////////////////

typedef struct _BufferSink {
    char* _Nullable buffer;
    size_t          capacity;
} BufferSink;

// Note that this sink continues to calculate the number of characters we would
// want to write even after we've reached the maximum size of the string buffer.
// See: <https://en.cppreference.com/w/c/io/fprintf>
static errno_t vprintf_buffer_sink(FormatterRef _Nonnull self, const char* _Nonnull pBuffer, size_t nBytes)
{
    BufferSink* pSink = (BufferSink*)self->context;

    if (pSink->capacity > 0 && pSink->buffer) {
        size_t nBytesToWrite;

        if (self->charactersWritten + nBytes > pSink->capacity) {
            const size_t nExcessBytes = (self->charactersWritten + nBytes) - pSink->capacity;
            nBytesToWrite = nBytes - nExcessBytes;
        }
        else {
            nBytesToWrite = nBytes;
        }

        memcpy(&pSink->buffer[self->charactersWritten], pBuffer, nBytesToWrite);
    }

    return 0;
}

int sprintf(char *buffer, const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vsprintf(buffer, format, ap);
    va_end(ap);
    return r;
}

int vsprintf(char *buffer, const char *format, va_list ap)
{
    Formatter fmt;
    BufferSink sink;

    sink.buffer = buffer;
    sink.capacity = (buffer) ? SIZE_MAX-1 : 0;
    __Formatter_Init(&fmt, vprintf_buffer_sink, &sink);

    const errno_t err = __Formatter_vFormat(&fmt, format, ap);
    const size_t nchars = fmt.charactersWritten;
    __Formatter_Deinit(&fmt);

    if (err == 0) {
        buffer[nchars] = '\0';
        return nchars;
     } else { 
        return -err;
     }
}

int snprintf(char *buffer, size_t bufsiz, const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vsnprintf(buffer, bufsiz, format, ap);
    va_end(ap);
    return r;
}

int vsnprintf(char *buffer, size_t bufsiz, const char *format, va_list ap)
{
    Formatter fmt;
    BufferSink sink;

    sink.buffer = buffer;
    sink.capacity = (buffer && bufsiz > 0) ? bufsiz - 1 : 0;
    __Formatter_Init(&fmt, vprintf_buffer_sink, &sink);
    
    const errno_t err = __Formatter_vFormat(&fmt, format, ap);
    const size_t nchars = fmt.charactersWritten;
    __Formatter_Deinit(&fmt);

    if (err == 0) {
        buffer[nchars] = '\0';
        return nchars;
     } else { 
        return -err;
     }
}


////////////////////////////////////////////////////////////////////////////////

#define INITIAL_MALLOC_SINK_BUFFER_CAPACITY 256
#define MIN_GROW_MALLOC_SINK_BUFFER_CAPACITY 128
typedef struct _MallocSink {
    char* _Nullable buffer;
    size_t          capacity;
} MallocSink;

// Dynamically allocates the memory for the string
static errno_t vaprintf_malloc_sink(FormatterRef _Nonnull self, const char* _Nonnull pBuffer, size_t nBytes)
{
    MallocSink* pSink = (MallocSink*)self->context;

    if (self->charactersWritten == pSink->capacity) {
        const size_t newCapacity = (pSink->capacity > 0) ? __max(nBytes, MIN_GROW_MALLOC_SINK_BUFFER_CAPACITY) + pSink->capacity : INITIAL_MALLOC_SINK_BUFFER_CAPACITY;
        char* pNewBuffer = (char*) malloc(newCapacity);

        if (pNewBuffer == NULL) {
            return -ENOMEM;
        }

        memcpy(pNewBuffer, pSink->buffer, self->charactersWritten);
        free(pSink->buffer);
        pSink->buffer = pNewBuffer;
    }

    memcpy(&pSink->buffer[self->charactersWritten], pBuffer, nBytes);

    return 0;
}

int asprintf(char **str_ptr, const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vasprintf(str_ptr, format, ap);
    va_end(ap);
    return r;
}

int vasprintf(char **str_ptr, const char *format, va_list ap)
{
    Formatter fmt;
    MallocSink sink;

    sink.buffer = NULL;
    sink.capacity = 0;
    __Formatter_Init(&fmt, vaprintf_malloc_sink, &sink);

    const errno_t err = __Formatter_vFormat(&fmt, format, ap);
    const size_t nchars = fmt.charactersWritten;
    __Formatter_Deinit(&fmt);

    if (err == 0) {
        sink.buffer[nchars] = '\0';
        *str_ptr = sink.buffer;
        return nchars;
     } else {
        *str_ptr = NULL; 
        return -err;
     }
}
