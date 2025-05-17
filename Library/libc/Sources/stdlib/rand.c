//
//  rand.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>


static unsigned int __gSeed = 1;

void srand(unsigned int seed)
{
    __gSeed = seed;
}

int rand(void)
{
    return rand_r(&__gSeed);
}
