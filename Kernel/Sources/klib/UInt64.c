//
//  UInt64.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Types.h"

const Character* _Nonnull UInt64_ToString(UInt64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength)
{
    static const char* gDigit = "0123456789abcdef";
    Character* p0 = &pString[__max(maxLength - fieldWidth - 1, 0)];
    Character *p = &pString[maxLength - 1];
    Int64 q, r;
    
    *p = '\0';
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
    
    return __max(p, p0);
}

UInt64 _divuint64_20(UInt64 dividend, UInt64 divisor)
{
    Int64 quo;
    
    Int64_DivMod(dividend, divisor, &quo, NULL);
    
    return (UInt64) quo;
}

UInt64 _moduint64_20(UInt64 dividend, UInt64 divisor)
{
    Int64 quo, rem;
    
    Int64_DivMod(dividend, divisor, &quo, &rem);
    
    return (UInt64) rem;
}
