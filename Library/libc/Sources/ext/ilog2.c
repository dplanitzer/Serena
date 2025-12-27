//
//  ext/ilog2.c
//  lib, libsc
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <ext/math.h>


unsigned int ul_log2(unsigned long n)
{
    unsigned long p = 1ul;
    unsigned int b = 0;

    while (p < n) {
        p <<= 1ul;
        b++;
    }
        
    return b;
}

unsigned int ull_log2(unsigned long long n)
{
    unsigned long long p = 1ull;
    unsigned int b = 0;
        
    while (p < n) {
        p <<= 1ull;
        b++;
    }
        
    return b;
}
