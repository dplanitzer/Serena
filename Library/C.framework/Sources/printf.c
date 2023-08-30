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
#include <System.h>
#include "printf.h"


#define FORMAT_MODIFIER_HALF_HALF   0
#define FORMAT_MODIFIER_HALF        1
#define FORMAT_MODIFIER_LONG        2
#define FORMAT_MODIFIER_LONG_LONG   3
#define FORMAT_MODIFIER_COUNT       4


// 8bit 16bit, 32bit, 64bit
static const int8_t gFieldWidth_Oct[FORMAT_MODIFIER_COUNT] = {3,  6, 11, 22};
static const int8_t gFieldWidth_Dec[FORMAT_MODIFIER_COUNT] = {3,  5, 10, 20};
static const int8_t gFieldWidth_Hex[FORMAT_MODIFIER_COUNT] = {2,  4,  8, 16};


static char parse_padding_char(const char *format, size_t * _Nonnull parsed_len)
{
    char ch = '\0';
    
    *parsed_len = 0;
    if (format[0] == '0') {
        ch = '0';
        *parsed_len = 1;
    }
    
    return ch;
}

static int parse_format_modifier(const char *format, size_t * _Nonnull parsed_len)
{
    int mod = FORMAT_MODIFIER_LONG;
    int i = 0;
    
    *parsed_len = 0;
    if (format[i] == 'l') {
        if (format[i + 1] == 'l') {
            mod = FORMAT_MODIFIER_LONG_LONG;
            *parsed_len = 2;
        } else {
            mod = FORMAT_MODIFIER_LONG;
            *parsed_len = 1;
        }
    } else if (format[i] == 'h') {
        if (format[i + 1] == 'h') {
            mod = FORMAT_MODIFIER_HALF_HALF;
            *parsed_len = 2;
        } else {
            mod = FORMAT_MODIFIER_HALF;
            *parsed_len = 1;
        }
    } else if (format[i] == 'j') {
        // intmax_t, uintmax_t
#if INTMAX_WIDTH == 32
        mod = FORMAT_MODIFIER_LONG;
#elif INTMAX_WIDTH == 64
        mod = FORMAT_MODIFIER_LONG_LONG;
#else
#error "unknown INTMAX_WIDTH"
#endif
        *parsed_len = 1;
    } else if (format[i] == 'z') {
        // ssize_t, size_t
#if __SIZE_WIDTH == 32
        mod = FORMAT_MODIFIER_LONG;
#elif __SIZE_WIDTH == 64
        mod = FORMAT_MODIFIER_LONG_LONG;
#else
#error "unknown __SIZE_WIDTH"
#endif
        *parsed_len = 1;
    } else if (format[i] == 't') {
        // ptrdiff_t
#if __PTRDIFF_WIDTH == 32
        mod = FORMAT_MODIFIER_LONG;
#elif __PTRDIFF_WIDTH == 64
        mod = FORMAT_MODIFIER_LONG_LONG;
#else
#error "unknown __PTRDIFF_WIDTH"
#endif
        *parsed_len = 1;
    }
    
    return mod;
}

static int64_t get_arg_as_int64(int modifier, va_list* _Nonnull ap)
{
    switch (modifier) {
        case FORMAT_MODIFIER_LONG_LONG:
            return va_arg(*ap, int64_t);
            
        case FORMAT_MODIFIER_LONG:
            return (int64_t) va_arg(*ap, int32_t);
            
        case FORMAT_MODIFIER_HALF:
            return (int64_t) (int16_t)va_arg(*ap, int32_t);
            
        case FORMAT_MODIFIER_HALF_HALF:
            return (int64_t) (int8_t)va_arg(*ap, int32_t);
            
        default:
            abort();
            return 0; // not reached
    }
}

static uint64_t get_arg_as_uint64(int modifier, va_list* _Nonnull ap)
{
    switch (modifier) {
        case FORMAT_MODIFIER_LONG_LONG:
            return va_arg(*ap, uint64_t);
            
        case FORMAT_MODIFIER_LONG:
            return (uint64_t) va_arg(*ap, uint32_t);
            
        case FORMAT_MODIFIER_HALF:
            return (uint64_t) (uint16_t)va_arg(*ap, uint32_t);
            
        case FORMAT_MODIFIER_HALF_HALF:
            return (uint64_t) (uint8_t)va_arg(*ap, uint32_t);
            
        default:
            abort();
            return 0; // not reached
    }
}

static void write_characters_written(CharacterStream* _Nonnull pStream, int modifier, void* _Nonnull ptr)
{
    switch (modifier) {
        case FORMAT_MODIFIER_HALF_HALF:
            *((signed char*) ptr) = (signed char) pStream->charactersWritten;
            break;

        case FORMAT_MODIFIER_HALF:
            *((signed short*) ptr) = (signed short) pStream->charactersWritten;
            break;

        case FORMAT_MODIFIER_LONG:
            *((signed int*) ptr) = (signed int) pStream->charactersWritten;
            break;

        case FORMAT_MODIFIER_LONG_LONG:
            *((signed long long*) ptr) = (signed long long) pStream->charactersWritten;
            break;

        default:
            abort();
            break;
    }
}


void __vprintf_init(CharacterStream* _Nonnull pStream, PrintSink_Func _Nonnull pSinkFunc, void * _Nullable pContext)
{
    pStream->sinkFunc = pSinkFunc;
    pStream->context = pContext;
    pStream->charactersWritten = 0;
    pStream->bufferCapacity = STREAM_BUFFER_CAPACITY - 1;   // reserve the last character for the '\0' (end of string marker)
    pStream->bufferCount = 0;
}

static errno_t __vprintf_flush(CharacterStream* _Nonnull pStream)
{
    errno_t err = EOK;

    if (pStream->bufferCount > 0) {
        pStream->buffer[pStream->bufferCount] = '\0';
        err = pStream->sinkFunc(pStream, pStream->buffer, pStream->bufferCount);
        if (err == EOK) pStream->charactersWritten += pStream->bufferCount;
        pStream->bufferCount = 0;
    }
    return err;
}

static errno_t __vprintf_string(CharacterStream* _Nonnull pStream, const char *str)
{
    errno_t err = __vprintf_flush(pStream);

    if (err == EOK) {
        const size_t len = strlen(str);

        if (len > 0) {
            err = pStream->sinkFunc(pStream, str, len);
            if (err == EOK) pStream->charactersWritten += len;
        }
    }

    return err;
}

// Triggers a return on failure
#define __vprintf_char(__p, __ch) \
    if (__p->bufferCount == __p->bufferCapacity) { \
        const errno_t err = __vprintf_flush(__p); \
        if (err != EOK) { return err; } \
    } \
    __p->buffer[__p->bufferCount++] = __ch;


// Triggers a return on failure
#define fail_err(f) \
    { \
        const errno_t err = (f); \
        if (err != EOK) { return err; } \
    }


errno_t __vprintf(CharacterStream* _Nonnull pStream, const char* _Nonnull format, va_list ap)
{
    size_t parsedLen;
    bool done = false;
    
    while (!done) {
        char ch = *format++;
        
        switch (ch) {
            case '\0':
                done = true;
                break;
                
            case '\\':
                ch = *format++;
                switch (ch) {
                    case '\0':
                        done = true;
                        break;
                        
                    default:
                        __vprintf_char(pStream, ch);
                        break;
                }
                break;
                
            case '%': {
                const char paddingChar = parse_padding_char(format, &parsedLen);
                format += parsedLen;
                const int modifier = parse_format_modifier(format, &parsedLen);
                format += parsedLen;
                
                ch = *format++;
                switch (ch) {
                    case '\0':
                        done = true;
                        break;
                        
                    case 'c':
                        __vprintf_char(pStream, (unsigned char) va_arg(ap, int));
                        break;
                        
                    case 's':
                        fail_err(__vprintf_string(pStream, va_arg(ap, const char*)));
                        break;
                        
                    case 'o':
                        fail_err(__vprintf_string(pStream, __ulltoa(get_arg_as_uint64(modifier, &ap), 8, false, gFieldWidth_Oct[modifier], paddingChar, pStream->buffer, pStream->bufferCapacity)));
                        break;
                        
                    case 'u':
                        fail_err(__vprintf_string(pStream, __ulltoa(get_arg_as_uint64(modifier, &ap), 10, false, gFieldWidth_Dec[modifier], paddingChar, pStream->buffer, pStream->bufferCapacity)));
                        break;
                        
                    case 'd':
                    case 'i':
                        fail_err(__vprintf_string(pStream, __lltoa(get_arg_as_int64(modifier, &ap), 10, false, gFieldWidth_Dec[modifier], paddingChar, pStream->buffer, pStream->bufferCapacity)));
                        break;
                        
                    case 'x':
                        fail_err(__vprintf_string(pStream, __ulltoa(get_arg_as_uint64(modifier, &ap), 16, false, gFieldWidth_Hex[modifier], paddingChar, pStream->buffer, pStream->bufferCapacity)));
                        break;

                    case 'X':
                        fail_err(__vprintf_string(pStream, __ulltoa(get_arg_as_uint64(modifier, &ap), 16, true, gFieldWidth_Hex[modifier], paddingChar, pStream->buffer, pStream->bufferCapacity)));
                        break;

                    case 'p':
                        fail_err(__vprintf_string(pStream, __ulltoa(va_arg(ap, uintptr_t), 16, false, 8, '0', pStream->buffer, pStream->bufferCapacity)));
                        break;

                    case 'n':
                        write_characters_written(pStream, modifier, (void*)va_arg(ap, void*));
                        break;

                    default:
                        __vprintf_char(pStream, ch);
                        break;
                }
                break;
            }
                
            default:
                __vprintf_char(pStream, ch);
                break;
        }
    }

    fail_err(__vprintf_flush(pStream));

    return EOK;
}


////////////////////////////////////////////////////////////////////////////////

int printf(const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vprintf(format, ap);
    va_end(ap);
    return r;
}

static int vprintf_console_sink(CharacterStream* _Nullable pStream, const char* _Nonnull pBuffer, size_t nBytes)
{
    return __write(pBuffer);
}

int vprintf(const char *format, va_list ap)
{
    CharacterStream stream;

    __vprintf_init(&stream, vprintf_console_sink, NULL);
    const errno_t err = __vprintf(&stream, format, ap);
    return (err == EOK) ? stream.charactersWritten : -err;
}


////////////////////////////////////////////////////////////////////////////////

typedef struct _BufferSink {
    char* _Nullable buffer;
    size_t          maxCharactersToWrite;
} BufferSink;

// Note that this sink continues to calculate the number of characters we would
// want to write even after we've reached the maximum size of the string buffer.
// See: <https://en.cppreference.com/w/c/io/fprintf>
static int vprintf_buffer_sink(CharacterStream* _Nonnull pStream, const char* _Nonnull pBuffer, size_t nBytes)
{
    BufferSink* pSink = (BufferSink*)pStream->context;

    if (pSink->maxCharactersToWrite > 0 && pSink->buffer) {
        size_t nBytesToWrite;

        if (pStream->charactersWritten + nBytes > pSink->maxCharactersToWrite) {
            const size_t nExcessBytes = (pStream->charactersWritten + nBytes) - pSink->maxCharactersToWrite;
            nBytesToWrite = nBytes - nExcessBytes;
        }
        else {
            nBytesToWrite = nBytes;
        }

        memcpy(&pSink->buffer[pStream->charactersWritten], pBuffer, nBytesToWrite);
    }

    return EOK;
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
    CharacterStream stream;
    BufferSink sink;

    sink.buffer = buffer;
    sink.maxCharactersToWrite = (buffer) ? SIZE_MAX : 0;
    __vprintf_init(&stream, vprintf_buffer_sink, &sink);
    const errno_t err = __vprintf(&stream, format, ap);
    if (err == EOK) {
        buffer[stream.charactersWritten] = '\0';
        return stream.charactersWritten;
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
    CharacterStream stream;
    BufferSink sink;

    sink.buffer = buffer;
    sink.maxCharactersToWrite = (buffer && bufsiz > 0) ? bufsiz - 1 : 0;
    __vprintf_init(&stream, vprintf_buffer_sink, &sink);
    const errno_t err = __vprintf(&stream, format, ap);
    if (err == EOK) {
        buffer[stream.charactersWritten] = '\0';
        return stream.charactersWritten;
     } else { 
        return -err;
     }
}
