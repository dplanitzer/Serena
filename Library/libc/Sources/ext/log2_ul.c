//
//  ext/log2_ul.c
//  lib, libsc
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <ext/bit.h>


unsigned int log2_ul(unsigned long n)
{
    unsigned long p = 1ul;
    unsigned int b = 0;

    while (p < n) {
        p <<= 1ul;
        b++;
    }
        
    return b;
}
