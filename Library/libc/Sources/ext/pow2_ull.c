//
//  ext/pow2_ull.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <ext/bit.h>


bool ispow2_ull(unsigned long long n)
{
    return (n && (n & (n - 1ull)) == 0ull) ? true : false;
}

unsigned long long pow2_ceil_ull(unsigned long long n)
{
    if (n && !(n & (n - 1))) {
        return n;
    } else {
        unsigned long long p = 1;
        
        while (p < n) {
            p <<= 1;
        }
        
        return p;
    }
}
