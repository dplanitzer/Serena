//
//  rand.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>

static int gSeed = 1;

void srand(unsigned int seed)
{
    gSeed = seed;
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
    const int hi = gSeed / q;
    const int lo = gSeed % q;
    const int test = a * lo - r * hi;

    gSeed = (test > 0) ? test : test + m;
    return gSeed;   // [0, RAND_MAX]
}
