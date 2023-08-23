//
//  Int.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Runtime.h"

Int Int_NextPowerOf2(Int n)
{
    if (n && !(n & (n - 1))) {
        return n;
    } else {
        Int p = 1;

        while (p < n) {
            p <<= 1;
        }
    
        return p;
    }
}
