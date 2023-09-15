//
//  String.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Types.h"


// Copies the characters of 'pSrc' to 'pDst'. Returns a pointer that points to
// the first byte past the '\0' byte in the destination string. 
Character* _Nonnull String_Copy(Character* _Nonnull pDst, const Character* _Nonnull pSrc)
{
    while (*pSrc != '\0') {
        *pDst++ = *pSrc++;
    }
    *pDst++ = '\0';

    return pDst;
}

ByteCount String_Length(const Character* _Nonnull pStr)
{
    const Character* p = pStr;

    while(*p++ != '\0');

    return p - pStr - 1;
}

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
