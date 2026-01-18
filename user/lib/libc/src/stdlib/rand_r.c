//
//  rand_r.c
//  libc
//
//  Created by Dietmar Planitzer on 2/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>


#if 1
// SHR3 random number generator
int rand_r(unsigned int *seed)
{
    int shr3;

    shr3 = *seed;
    if (shr3 == 0) {
        shr3 = -1;
    }
    shr3 ^= (shr3 << 13);
    shr3 ^= (shr3 >> 17);
    shr3 ^= (shr3 << 5);
    
    if (shr3 < 0) {
        shr3 += 2147483647;
    }
    *seed = shr3;

    return shr3;
}
#else
// Based on:
// Random number generators: good ones are hard to find
// ACM October 1988 Volume 31 Number 10
// Page 1195
int rand_r(unsigned int *seed)
{
    int iSeed = *((int*)seed);
    const int a = 16807;
    const int m = 2147483647;
    const int q = 127773;
    const int r = 2836;
    const int hi = iSeed / q;
    const int lo = iSeed % q;
    const int test = a * lo - r * hi;

    iSeed = (test > 0) ? test : test + m;
    *seed = (unsigned int)iSeed;

    return iSeed;   // [0, RAND_MAX]
}
#endif