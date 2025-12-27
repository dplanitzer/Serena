//
//  ext/ipow2.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <ext/math.h>


bool ul_ispow2(unsigned long n)
{
    return (n && (n & (n - 1ul)) == 0ul) ? true : false;
}

bool ull_ispow2(unsigned long long n)
{
    return (n && (n & (n - 1ull)) == 0ull) ? true : false;
}

unsigned long ul_pow2_ceil(unsigned long n)
{
    if (n && !(n & (n - 1))) {
        return n;
    } else {
        unsigned long p = 1;
        
        while (p < n) {
            p <<= 1;
        }
        
        return p;
    }
}

unsigned long long ull_pow2_ceil(unsigned long long n)
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
