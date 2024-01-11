//
//  UInt.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Types.h"

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
