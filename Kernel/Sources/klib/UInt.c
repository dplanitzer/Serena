//
//  UInt.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Types.h"

const Character* _Nonnull UInt64_ToString(UInt64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength)
{
    static const char* gDigit = "0123456789abcdef";
    Character* p0 = &pString[__max(maxLength - fieldWidth - 1, 0)];
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
    
    return __max(p, p0);
}

UInt UInt_NextPowerOf2(UInt n)
{
    if (n && !(n & (n - 1))) {
        return n;
    } else {
        UInt p = 1;
        
        while (p < n) {
            p <<= 1;
        }
        
        return p;
    }
}
