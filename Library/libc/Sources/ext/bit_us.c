//
//  ext/bit_us.c
//  lib, libsc
//
//  Created by Dietmar Planitzer on 1/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/bit.h>


// Based on the book Hacker's Delight, 2nd Edition by Henry S Warren, Jr.
unsigned int leading_zeros_us(unsigned short val)
{
    unsigned int n;

    if (val == 0) return 16;
    n = 0;
    if (val <= 0x00FF) {n = n +  8; val <<=  8;}
    if (val <= 0x0FFF) {n = n +  4; val <<=  4;}
    if (val <= 0x3FFF) {n = n +  2; val <<=  2;}
    if (val <= 0x7FFF) {n = n +  1;}
    
    return n;
}

unsigned int leading_ones_us(unsigned short val)
{
    return leading_zeros_us(~val);
}
