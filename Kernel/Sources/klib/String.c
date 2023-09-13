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
