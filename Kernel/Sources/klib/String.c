//
//  String.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Types.h"


ByteCount String_Length(const Character* _Nonnull pStr)
{
    const Character* p = pStr;

    while(*p++ != '\0');

    return p - pStr - 1;
}

ByteCount String_LengthUpTo(const Character* _Nonnull pStr, ByteCount strsz)
{
    ByteCount len = 0;

    while (*pStr++ != '\0' && len < strsz) {
        len++;
    }

    return len;
}

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

Character* _Nonnull String_CopyUpTo(Character* _Nonnull pDst, const Character* _Nonnull pSrc, ByteCount count)
{
    while (*pSrc != '\0' && count > 0) {
        *pDst++ = *pSrc++;
        count--;
    }
    if (count > 0) {
        *pDst++ = '\0';
    }

    return pDst;
}

Bool String_Equals(const Character* _Nonnull pLhs, const Character* _Nonnull pRhs)
{
    while (*pLhs != '\0' && *pLhs == *pRhs) {
        pLhs++;
        pRhs++;
    }

    return (*pLhs == *pRhs) ? true : false;
}

Bool String_EqualsUpTo(const Character* _Nonnull pLhs, const Character* _Nonnull pRhs, ByteCount count)
{
    while (count > 0 && *pLhs != '\0' && *pLhs == *pRhs) {
        pLhs++;
        pRhs++;
        count--;
    }

    return (count == 0 || *pLhs == *pRhs) ? true : false;
}
