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


// 8bit 16bit, 32bit, 64bit
static const int8_t gFieldWidth_Bin[4] = {8, 16, 32, 64};
static const int8_t gFieldWidth_Oct[4] = {3,  6, 11, 22};
static const int8_t gFieldWidth_Dec[4] = {3,  5, 10, 20};
static const int8_t gFieldWidth_Hex[4] = {2,  4,  8, 16};


#define FORMAT_MODIFIER_HALF_HALF   0
#define FORMAT_MODIFIER_HALF        1
#define FORMAT_MODIFIER_LONG        2
#define FORMAT_MODIFIER_LONG_LONG   3


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
    }
    
    return mod;
}

static int64_t get_promoted_int_arg(int modifier, va_list * _Nonnull ap)
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

static uint64_t get_promoted_uint_arg(int modifier, va_list * _Nonnull ap)
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


#define BUFFER_CAPACITY 64
typedef struct _CharacterStream {
    PrintSink_Func _Nonnull sinkFunc;
    void* _Nullable         context;
    size_t                  bufferCount;
    size_t                  bufferCapacity;
    char                    buffer[BUFFER_CAPACITY];
} CharacterStream;


static errno_t __vprintf_flush(CharacterStream* _Nonnull pStream)
{
    errno_t err = EOK;

    if (pStream->bufferCount > 0) {
        pStream->buffer[pStream->bufferCount] = '\0';
        err = pStream->sinkFunc(pStream->context, pStream->buffer, pStream->bufferCount);
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
            err = pStream->sinkFunc(pStream->context, str, len);
        }
    }

    return err;
}

// Triggers a return on failure
#define __vprintf_char(ch) \
    if (s.bufferCount == s.bufferCapacity) { \
        const errno_t err = __vprintf_flush(&s); \
        if (err != EOK) { return err; } \
    } \
    s.buffer[s.bufferCount++] = ch;


// Triggers a return on failure
#define fail_err(f) \
    { \
        const errno_t err = (f); \
        if (err != EOK) { return err; } \
    }


errno_t __vprintf(PrintSink_Func _Nonnull pSinkFunc, void* _Nullable pContext, const char* _Nonnull format, va_list ap)
{
    CharacterStream s;
    size_t parsedLen;
    bool done = false;
    
    s.sinkFunc = pSinkFunc;
    s.context = pContext;
    s.bufferCapacity = BUFFER_CAPACITY - 1;   // reserve the last character for the '\0' (end of string marker)
    s.bufferCount = 0;

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
                        __vprintf_char(ch);
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
                        __vprintf_char((unsigned char) va_arg(ap, int));
                        break;
                        
                    case 's':
                        fail_err(__vprintf_string(&s, va_arg(ap, const char*)));
                        break;
                        
                    case 'b':
                        fail_err(__vprintf_string(&s, __ulltoa(get_promoted_uint_arg(modifier, &ap), 2, gFieldWidth_Bin[modifier], paddingChar, s.buffer, s.bufferCapacity)));
                        break;
                        
                    case 'o':
                        fail_err(__vprintf_string(&s, __ulltoa(get_promoted_uint_arg(modifier, &ap), 8, gFieldWidth_Oct[modifier], paddingChar, s.buffer, s.bufferCapacity)));
                        break;
                        
                    case 'u':
                        fail_err(__vprintf_string(&s, __ulltoa(get_promoted_uint_arg(modifier, &ap), 10, gFieldWidth_Dec[modifier], paddingChar, s.buffer, s.bufferCapacity)));
                        break;
                        
                    case 'd':
                    case 'i':
                        fail_err(__vprintf_string(&s, __lltoa(get_promoted_int_arg(modifier, &ap), 10, gFieldWidth_Dec[modifier], paddingChar, s.buffer, s.bufferCapacity)));
                        break;
                        
                    case 'x':
                        fail_err(__vprintf_string(&s, __ulltoa(get_promoted_uint_arg(modifier, &ap), 16, gFieldWidth_Hex[modifier], paddingChar, s.buffer, s.bufferCapacity)));
                        break;

                    case 'p':
                        fail_err(__vprintf_string(&s, __ulltoa(va_arg(ap, uintptr_t), 16, 8, '0', s.buffer, s.bufferCapacity)));
                        break;

                    default:
                        __vprintf_char(ch);
                        break;
                }
                break;
            }
                
            default:
                __vprintf_char(ch);
                break;
        }
    }

    fail_err(__vprintf_flush(&s));

    return EOK;
}


////////////////////////////////////////////////////////////////////////////////

typedef struct _ConsoleSink {
    size_t  charactersWritten;
} ConsoleSink;

int printf(const char *format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    const int r = vprintf(format, ap);
    va_end(ap);
    return r;
}

static int vprintf_console_sink(void* _Nullable pContext, const char* _Nonnull pBuffer, size_t nBytes)
{
    ConsoleSink* pSink = (ConsoleSink*)pContext;
    const int err = __write(pBuffer);
    
    if (err == EOK) {
        pSink->charactersWritten += nBytes;
    }
    return err;
}

int vprintf(const char *format, va_list ap)
{
    ConsoleSink sink;

    sink.charactersWritten = 0;
    const errno_t err = __vprintf(vprintf_console_sink, &sink, format, ap);
    return (err == EOK) ? sink.charactersWritten : -err;
}


////////////////////////////////////////////////////////////////////////////////

typedef struct _BufferSink {
    char* _Nullable buffer;
    size_t          charactersWritten;
    size_t          maxCharactersToWrite;
} BufferSink;

// Note that this sink continues to calculate the number of characters we would
// want to write even after we've reached the maximum size of the string buffer.
// See: <https://en.cppreference.com/w/c/io/fprintf>
static int vprintf_buffer_sink(void* _Nullable pContext, const char* _Nonnull pBuffer, size_t nBytes)
{
    BufferSink* pSink = (BufferSink*)pContext;

    if (pSink->maxCharactersToWrite > 0 && pSink->buffer) {
        size_t nBytesToWrite;

        if (pSink->charactersWritten + nBytes > pSink->maxCharactersToWrite) {
            const size_t nExcessBytes = (pSink->charactersWritten + nBytes) - pSink->maxCharactersToWrite;
            nBytesToWrite = nBytes - nExcessBytes;
        }
        else {
            nBytesToWrite = nBytes;
        }

        memcpy(&pSink->buffer[pSink->charactersWritten], pBuffer, nBytesToWrite);
    }
    pSink->charactersWritten += nBytes;

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
    BufferSink sink;

    sink.buffer = buffer;
    sink.charactersWritten = 0;
    sink.maxCharactersToWrite = (buffer) ? SIZE_MAX : 0;
    const errno_t err = __vprintf(vprintf_buffer_sink, &sink, format, ap);
    if (err == EOK) {
        buffer[sink.charactersWritten] = '\0';
        return sink.charactersWritten;
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
    BufferSink sink;

    sink.buffer = buffer;
    sink.charactersWritten = 0;
    sink.maxCharactersToWrite = (buffer && bufsiz > 0) ? bufsiz - 1 : 0;
    const errno_t err = __vprintf(vprintf_buffer_sink, &sink, format, ap);
    if (err == EOK) {
        buffer[sink.charactersWritten] = '\0';
        return sink.charactersWritten;
     } else { 
        return -err;
     }
}
