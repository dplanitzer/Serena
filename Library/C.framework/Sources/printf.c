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
    size_t                  characterCount;
    size_t                  characterCapacity;
    char                    buffer[BUFFER_CAPACITY];
} CharacterStream;


static int __vprintf_flush(CharacterStream* _Nonnull pStream)
{
    int nwritten = 0;

    if (pStream->characterCount > 0) {
        pStream->buffer[pStream->characterCount] = '\0';
        nwritten = pStream->sinkFunc(pStream->context, pStream->buffer);
        pStream->characterCount = 0;
    }
    return nwritten;
}

static int __vprintf_string(CharacterStream* _Nonnull pStream, const char *str)
{
    const int nwritten = __vprintf_flush(pStream);

    if (nwritten >= 0) {
        const int nwritten2 = pStream->sinkFunc(pStream->context, str);

        if (nwritten2 >= 0) {
            return nwritten + nwritten2;
        }
    }
    return EOF;
}

#define __vprintf_char(ch) \
    if (s.characterCount == s.characterCapacity) { \
        __vprintf_flush(&s); \
    } \
    s.buffer[s.characterCount++] = ch;


#define __nwritten(f) \
    { \
        const int nwritten2 = (f); \
        if (nwritten < 0) { return EOF; } \
        nwritten += nwritten2; \
    }


int __vprintf(PrintSink_Func _Nonnull pSinkFunc, void* _Nullable pContext, const char* _Nonnull format, va_list ap)
{
    size_t parsedLen;
    CharacterStream s;
    int nwritten = 0;
    bool done = false;
    
    s.sinkFunc = pSinkFunc;
    s.context = pContext;
    s.characterCapacity = BUFFER_CAPACITY - 1;   // reserve the last character for the '\0' (end of string marker)
    s.characterCount = 0;

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
                        __vprintf_char(va_arg(ap, int));
                        break;
                        
                    case 's':
                        __nwritten(__vprintf_string(&s, va_arg(ap, const char*)));
                        break;
                        
                    case 'b':
                        __nwritten(__vprintf_string(&s, __ulltoa(get_promoted_uint_arg(modifier, &ap), 2, gFieldWidth_Bin[modifier], paddingChar, s.buffer, BUFFER_CAPACITY)));
                        break;
                        
                    case 'o':
                        __nwritten(__vprintf_string(&s, __ulltoa(get_promoted_uint_arg(modifier, &ap), 8, gFieldWidth_Oct[modifier], paddingChar, s.buffer, BUFFER_CAPACITY)));
                        break;
                        
                    case 'u':
                        __nwritten(__vprintf_string(&s, __ulltoa(get_promoted_uint_arg(modifier, &ap), 10, gFieldWidth_Dec[modifier], paddingChar, s.buffer, BUFFER_CAPACITY)));
                        break;
                        
                    case 'd':
                    case 'i':
                        __nwritten(__vprintf_string(&s, __lltoa(get_promoted_int_arg(modifier, &ap), 10, gFieldWidth_Dec[modifier], paddingChar, s.buffer, BUFFER_CAPACITY)));
                        break;
                        
                    case 'x':
                        __nwritten(__vprintf_string(&s, __ulltoa(get_promoted_uint_arg(modifier, &ap), 16, gFieldWidth_Hex[modifier], paddingChar, s.buffer, BUFFER_CAPACITY)));
                        break;

                    case 'p':
                        __nwritten(__vprintf_string(&s, __ulltoa(va_arg(ap, uint32_t), 16, 8, '0', s.buffer, BUFFER_CAPACITY)));
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

    __nwritten(__vprintf_flush(&s));

    return nwritten;
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

static int vprintf_console_sink(void* _Nullable pContext, const char* _Nonnull pString)
{
    if (__write(pString) == 0) {
        return strlen(pString);
    } else {
        return EOF;
    }
}

int vprintf(const char *format, va_list ap)
{
    return __vprintf(vprintf_console_sink, NULL, format, ap);
}
