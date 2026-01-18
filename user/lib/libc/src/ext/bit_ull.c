//
//  ext/bit_ull.c
//  lib, libsc
//
//  Created by Dietmar Planitzer on 1/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/bit.h>


#if !defined(__M68K__)
unsigned int leading_zeros_ull(unsigned long long val)
{
    unsigned int n = leading_zeros_ul(val >> 32);
    if (n == 32) {
        n += leading_zeros_ul(val & 0xffffffff); 
    }
    return n;
}

unsigned int leading_ones_ull(unsigned long long val)
{
    return leading_zeros_ull(~val);
}
#endif
