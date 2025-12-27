//
//  Int.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <kern/types.h>
#include <kern/kernlib.h>


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

unsigned int ul_log2(unsigned long n)
{
    unsigned long p = 1ul;
    unsigned int b = 0;

    while (p < n) {
        p <<= 1ul;
        b++;
    }
        
    return b;
}

unsigned int ull_log2(unsigned long long n)
{
    unsigned long long p = 1ull;
    unsigned int b = 0;
        
    while (p < n) {
        p <<= 1ull;
        b++;
    }
        
    return b;
}
