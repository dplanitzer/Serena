//
//  UInt.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Types.h"

unsigned int UInt_NextPowerOf2(unsigned int n)
{
    if (n && !(n & (n - 1))) {
        return n;
    } else {
        unsigned int p = 1;
        
        while (p < n) {
            p <<= 1;
        }
        
        return p;
    }
}
