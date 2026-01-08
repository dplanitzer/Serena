//
//  ext/pow2_ul.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <ext/bit.h>


bool ispow2_ul(unsigned long n)
{
    return (n && (n & (n - 1ul)) == 0ul) ? true : false;
}

unsigned long pow2_ceil_ul(unsigned long n)
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
