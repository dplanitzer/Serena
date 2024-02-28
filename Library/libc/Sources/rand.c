//
//  rand.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>

static int __gSeed = 1;

void srand(unsigned int seed)
{
    __gSeed = seed;
}

// Based on:
// Random number generators: good ones are hard to find
// ACM October 1988 Volume 31 Number 10
// Page 1195
int rand(void)
{
    const int a = 16807;
    const int m = 2147483647;
    const int q = 127773;
    const int r = 2836;
    const int hi = __gSeed / q;
    const int lo = __gSeed % q;
    const int test = a * lo - r * hi;

    __gSeed = (test > 0) ? test : test + m;
    return __gSeed;   // [0, RAND_MAX]
}
