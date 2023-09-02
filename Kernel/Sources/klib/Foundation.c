//
//  Foundation.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Foundation.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Int
// MARK: -
////////////////////////////////////////////////////////////////////////////////

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


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: String
// MARK: -
////////////////////////////////////////////////////////////////////////////////

// String
Bool String_Equals(const Character* _Nonnull pLhs, const Character* _Nonnull pRhs)
{
    while (*pLhs != '\0') {
        if (*pLhs != *pRhs) {
            return false;
        }

        pLhs++;
        pRhs++;
    }

    return true;
}
