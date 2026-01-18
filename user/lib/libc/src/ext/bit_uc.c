//
//  ext/bit_uc.c
//  lib, libsc
//
//  Created by Dietmar Planitzer on 1/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/bit.h>


#if !defined(__M68K__)
// Based on the book Hacker's Delight, 2nd Edition by Henry S Warren, Jr.
unsigned int leading_zeros_uc(unsigned char val)
{
    unsigned int n;

    if (val == 0) return 8;
    n = 0;
    if (val <= 0x0F) {n = n +  4; val <<=  4;}
    if (val <= 0x3F) {n = n +  2; val <<=  2;}
    if (val <= 0x7F) {n = n +  1;}
    
    return n;
}

unsigned int leading_ones_uc(unsigned char val)
{
    return leading_zeros_uc(~val);
}
#endif
