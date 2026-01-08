//
//  ext/bit_ul.c
//  lib, libsc
//
//  Created by Dietmar Planitzer on 1/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/bit.h>


#if !defined(__M68K__)
// Based on the book Hacker's Delight, 2nd Edition by Henry S Warren, Jr.
unsigned int leading_zeros_ul(unsigned long val)
{
    unsigned int n;

    if (val == 0) return 32;
    n = 0;
    if (val <= 0x0000FFFF) {n = n + 16; val <<= 16;}
    if (val <= 0x00FFFFFF) {n = n +  8; val <<=  8;}
    if (val <= 0x0FFFFFFF) {n = n +  4; val <<=  4;}
    if (val <= 0x3FFFFFFF) {n = n +  2; val <<=  2;}
    if (val <= 0x7FFFFFFF) {n = n +  1;}
    
    return n;
}

unsigned int leading_ones_ul(unsigned long val)
{
    return leading_zeros_ul(~val);
}
#endif
