//
//  ext/log2_ull.c
//  lib, libsc
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <ext/bit.h>


unsigned int log2_ull(unsigned long long n)
{
    unsigned long long p = 1ull;
    unsigned int b = 0;
        
    while (p < n) {
        p <<= 1ull;
        b++;
    }
        
    return b;
}
