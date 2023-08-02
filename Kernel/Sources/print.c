//
//  print.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/17/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Foundation.h"
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

static void vprint_locked(Console* _Nonnull pConsole, const Character* _Nonnull format, va_list ap)
{
    Int parsed_len;
    Character buf[32];
    Bool done = false;
    
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
                        Console_DrawCharacter(pConsole, ch);
                        break;
                }
                break;
                
            case '%': {
                const Character paddingChar = parse_padding_char(format, &parsed_len);
                format += parsed_len;
                const Int modifier = parse_format_modifier(format, &parsed_len);
                format += parsed_len;
                
                ch = *format++;
                switch (ch) {
                    case '\0':
                        done = true;
                        break;
                        
                    case 'c':
                        Console_DrawCharacter(pConsole, va_arg(ap, int));
                        break;
                        
                    case 's':
                        Console_DrawString(pConsole, va_arg(ap, const Character*));
                        break;
                        
                    case 'b':
                        Console_DrawString(pConsole, UInt64_ToString(get_promoted_uint_arg(modifier, &ap), 2, gFieldWidth_Bin[modifier], paddingChar, buf, sizeof(buf)));
                        break;
                        
                    case 'o':
                        Console_DrawString(pConsole, UInt64_ToString(get_promoted_uint_arg(modifier, &ap), 8, gFieldWidth_Oct[modifier], paddingChar, buf, sizeof(buf)));
                        break;
                        
                    case 'u':
                        Console_DrawString(pConsole, UInt64_ToString(get_promoted_uint_arg(modifier, &ap), 10, gFieldWidth_Dec[modifier], paddingChar, buf, sizeof(buf)));
                        break;
                        
                    case 'd':
                    case 'i':
                        Console_DrawString(pConsole, Int64_ToString(get_promoted_int_arg(modifier, &ap), 10, gFieldWidth_Dec[modifier], paddingChar, buf, sizeof(buf)));
                        break;
                        
                    case 'x':
                        Console_DrawString(pConsole, UInt64_ToString(get_promoted_uint_arg(modifier, &ap), 16, gFieldWidth_Hex[modifier], paddingChar, buf, sizeof(buf)));
                        break;

                    case 'p':
                        Console_DrawString(pConsole, UInt64_ToString(va_arg(ap, UInt32), 16, 8, '0', buf, sizeof(buf)));
                        break;

                    default:
                        Console_DrawCharacter(pConsole, ch);
                        break;
                }
                break;
            }
                
            default:
                Console_DrawCharacter(pConsole, ch);
                break;
        }
    }
}

static Console* gConsole;

// Initializes the print subsystem.
void print_init(void)
{
    Lock_Init(&gLock);
    gConsole = (Console*) DriverManager_GetDriverForName(gDriverManager, kConsoleName);
    assert(gConsole != NULL);
}

// Print formatted
void print(const Character* _Nonnull format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    vprint(format, ap);
    va_end(ap);
}

void vprint(const Character* _Nonnull format, va_list ap)
{
    decl_try_err();
    
    try(Lock_Lock(&gLock));
    vprint_locked(gConsole, format, ap);
    Lock_Unlock(&gLock);
    return;

catch:
    return;
}
