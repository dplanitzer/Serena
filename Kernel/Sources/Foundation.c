//
//  Foundation.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Foundation.h"
#include "Console.h"
#include "GraphicsDriver.h"
#include "Platform.h"

extern void _GraphicsDriver_SetClutEntry(Int index, UInt16 color);


void print(const Character* _Nonnull format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    Console_vDrawStringWithFormat(Console_GetMain(), format, ap);
    va_end(ap);
}

_Noreturn fatalError(const Character* _Nonnull filename, int line)
{
    Console* pConsole = Console_GetMain();
    Character buf[32];
    
    cpu_disable_irqs();
    chipset_stop_quantum_timer();

    _GraphicsDriver_SetClutEntry(0, 0x0000);
    _GraphicsDriver_SetClutEntry(1, 0x0f00);

    Console_DrawString(pConsole, "\n*** ");
    Console_DrawString(pConsole, filename);
    if (line > 0) {
        Console_DrawString(pConsole, ":");
        Console_DrawString(pConsole, Int64_ToString((Int64)line, 10, 4, '\0', buf, sizeof(buf)));
    }
    
    while (true);
}

static const char* gDigit = "0123456789abcdef";

const Character* _Nonnull Int64_ToString(Int64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength)
{
    Character* p0 = &pString[max(maxLength - fieldWidth - 1, 0)];
    Character *p = &pString[maxLength - 1];
    Int64 absval = (val < 0) ? -val : val;
    Int64 q, r;
    
    if (val < 0) {
        p0--;
    }
    
    pString[maxLength - 1] = '\0';
    do {
        p--;
        Int64_DivMod(absval, base, &q, &r);
        *p = (gDigit[r]);
        absval = q;
    } while (absval && p >= p0);
    
    if (val < 0) {
        p--;
        *p = '-';
        // Convert a zero padding char to a space 'cause zero padding doesn't make
        // sense if the number is negative.
        if (paddingChar == '0') {
            paddingChar = ' ';
        }
    }
    
    if (paddingChar != '\0') {
        while (p > p0) {
            p--;
            *p = paddingChar;
        }
    }
    
    return max(p, p0);
}

const Character* _Nonnull UInt64_ToString(UInt64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength)
{
    Character* p0 = &pString[max(maxLength - fieldWidth - 1, 0)];
    Character *p = &pString[maxLength - 1];
    Int64 q, r;
    
    pString[maxLength - 1] = '\0';
    do {
        p--;
        Int64_DivMod(val, base, &q, &r);
        *p = (gDigit[r]);
        val = q;
    } while (val && p >= p0);
    
    if (paddingChar != '\0') {
        while (p > p0) {
            p--;
            *p = paddingChar;
        }
    }
    
    return max(p, p0);
}
