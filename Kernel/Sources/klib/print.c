//
//  print.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/17/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <klib/klib.h>
#include "Console.h"
#include "DriverManager.h"

static Lock gLock;

// 8bit 16bit, 32bit, 64bit
static const Int8 gFieldWidth_Bin[4] = {8, 16, 32, 64};
static const Int8 gFieldWidth_Oct[4] = {3,  6, 11, 22};
static const Int8 gFieldWidth_Dec[4] = {3,  5, 10, 20};
static const Int8 gFieldWidth_Hex[4] = {2,  4,  8, 16};


#define FORMAT_MODIFIER_HALF_HALF   0
#define FORMAT_MODIFIER_HALF        1
#define FORMAT_MODIFIER_LONG        2
#define FORMAT_MODIFIER_LONG_LONG   3


static Character parse_padding_char(const Character* format, Int* parsed_len)
{
    Character ch = '\0';
    
    *parsed_len = 0;
    if (format[0] == '0') {
        ch = '0';
        *parsed_len = 1;
    }
    
    return ch;
}

static Int parse_format_modifier(const Character* format, Int* parsed_len)
{
    Int mod = FORMAT_MODIFIER_LONG;
    Int i = 0;
    
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

static Int64 get_promoted_int_arg(Int modifier, va_list* ap)
{
    switch (modifier) {
        case FORMAT_MODIFIER_LONG_LONG:
            return va_arg(*ap, Int64);
            
        case FORMAT_MODIFIER_LONG:
            return (Int64) va_arg(*ap, Int32);
            
        case FORMAT_MODIFIER_HALF:
            return (Int64) (Int16)va_arg(*ap, Int32);
            
        case FORMAT_MODIFIER_HALF_HALF:
            return (Int64) (Int8)va_arg(*ap, Int32);
            
        default:
            abort();
            return 0; // not reached
    }
}

static UInt64 get_promoted_uint_arg(Int modifier, va_list* ap)
{
    switch (modifier) {
        case FORMAT_MODIFIER_LONG_LONG:
            return va_arg(*ap, UInt64);
            
        case FORMAT_MODIFIER_LONG:
            return (UInt64) va_arg(*ap, UInt32);
            
        case FORMAT_MODIFIER_HALF:
            return (UInt64) (Int16)va_arg(*ap, UInt32);
            
        case FORMAT_MODIFIER_HALF_HALF:
            return (UInt64) (UInt8)va_arg(*ap, UInt32);
            
        default:
            abort();
            return 0; // not reached
    }
}

typedef struct _CharacterStream {
    PrintSink_Func _Nonnull sinkFunc;
    void* _Nullable         context;
    Character* _Nonnull     buffer;
    Int                     characterCount;
    Int                     characterCapacity;
} CharacterStream;

static void _printv_flush(CharacterStream* _Nonnull pStream)
{
    if (pStream->characterCount > 0) {
        pStream->buffer[pStream->characterCount] = '\0';
        pStream->sinkFunc(pStream->context, pStream->buffer);
        pStream->characterCount = 0;
    }
}

#define _printv_string(str) \
    _printv_flush(&s); \
    s.sinkFunc(s.context, str);

#define _printv_char(ch) \
    if (s.characterCount == s.characterCapacity) { \
        _printv_flush(&s); \
    } \
    s.buffer[s.characterCount++] = ch;


void _printv(PrintSink_Func _Nonnull pSinkFunc, void* _Nullable pContext, Character* _Nonnull pBuffer, Int bufferCapacity, const Character* _Nonnull format, va_list ap)
{
    Int parsedLen;
    CharacterStream s;
    Bool done = false;
    
    s.sinkFunc = pSinkFunc;
    s.context = pContext;
    s.buffer = pBuffer;
    s.characterCapacity = bufferCapacity - 1;   // reserve the last character for the '\0' (end of string marker)
    s.characterCount = 0;

    while (!done) {
        Character ch = *format++;
        
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
                        _printv_char(ch);
                        break;
                }
                break;
                
            case '%': {
                const Character paddingChar = parse_padding_char(format, &parsedLen);
                format += parsedLen;
                const Int modifier = parse_format_modifier(format, &parsedLen);
                format += parsedLen;
                
                ch = *format++;
                switch (ch) {
                    case '\0':
                        done = true;
                        break;
                        
                    case 'c':
                        _printv_char((Character) va_arg(ap, int));
                        break;
                        
                    case 's':
                        _printv_string(va_arg(ap, const Character*));
                        break;
                        
                    case 'b':
                        _printv_string(UInt64_ToString(get_promoted_uint_arg(modifier, &ap), 2, gFieldWidth_Bin[modifier], paddingChar, pBuffer, bufferCapacity));
                        break;
                        
                    case 'o':
                        _printv_string(UInt64_ToString(get_promoted_uint_arg(modifier, &ap), 8, gFieldWidth_Oct[modifier], paddingChar, pBuffer, bufferCapacity));
                        break;
                        
                    case 'u':
                        _printv_string(UInt64_ToString(get_promoted_uint_arg(modifier, &ap), 10, gFieldWidth_Dec[modifier], paddingChar, pBuffer, bufferCapacity));
                        break;
                        
                    case 'd':
                    case 'i':
                        _printv_string(Int64_ToString(get_promoted_int_arg(modifier, &ap), 10, gFieldWidth_Dec[modifier], paddingChar, pBuffer, bufferCapacity));
                        break;
                        
                    case 'x':
                        _printv_string(UInt64_ToString(get_promoted_uint_arg(modifier, &ap), 16, gFieldWidth_Hex[modifier], paddingChar, pBuffer, bufferCapacity));
                        break;

                    case 'p':
                        _printv_string(UInt64_ToString(va_arg(ap, UInt32), 16, 8, '0', pBuffer, bufferCapacity));
                        break;

                    default:
                        _printv_char(ch);
                        break;
                }
                break;
            }
                
            default:
                _printv_char(ch);
                break;
        }
    }

    _printv_flush(&s);
}


////////////////////////////////////////////////////////////////////////////////

#define PRINT_BUFFER_CAPACITY   80

static Console*     gConsole;
static Character    gPrintBuffer[PRINT_BUFFER_CAPACITY];


// Initializes the print subsystem.
void print_init(void)
{
    Lock_Init(&gLock);
    gConsole = DriverManager_GetDriverForName(gDriverManager, kConsoleName);
    assert(gConsole != NULL);
}

// Print formatted
void print(const Character* _Nonnull format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    printv(format, ap);
    va_end(ap);
}

static void printv_console_sink_locked(void* _Nullable pContext, const Character* _Nonnull pString)
{
    Console_DrawString(gConsole, pString);
}

void printv(const Character* _Nonnull format, va_list ap)
{
    Lock_Lock(&gLock);
    _printv(printv_console_sink_locked, NULL, gPrintBuffer, PRINT_BUFFER_CAPACITY, format, ap);
    Lock_Unlock(&gLock);
}
