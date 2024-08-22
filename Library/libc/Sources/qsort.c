//
//  qsort.c
//  libc
//
//  Created by Dietmar Planitzer on 8/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <__stddef.h>

typedef int (*compare_to_t)(const void*, const void*);
typedef void (*swap_t)(void*, void*, size_t);


static void __swap_1b(uint8_t* _Nonnull p1, uint8_t* _Nonnull p2, size_t size) 
{
    if (p1 != p2) {
        while (size-- > 0) {
            const uint8_t tmp = *p1;

            *p1 = *p2;
            *p2 = tmp;
            p1++; p2++;
        }
    }
}

static void __swap_4b(uint32_t* _Nonnull p1, uint32_t* _Nonnull p2, size_t size) 
{
    if (p1 != p2) {
        size_t size4 = size >> 2;

        while (size4-- > 0) {
            const uint32_t tmp = *p1;

            *p1 = *p2;
            *p2 = tmp;
            p1++; p2++;
        }
    }
}


// Based on "Quicksort with 3-way partitioning" from the book Algorithms 4th Edition by Robert Sedgewick and Kevin Wayne, pages 299ff.
static void __qsort(uint8_t* _Nonnull values, size_t l, size_t r, size_t size, compare_to_t _Nonnull comp, swap_t _Nonnull swap) 
{
    if (r <= l) {
        return;
    }

    size_t lt = l, i = l+1, gt = r;
    uint8_t* v = values + l * size;

    while (i <= gt) {
        int cmp = comp(values + i * size, v);

        if (cmp < 0) {
            uint8_t* p1 = values + lt * size;
            uint8_t* p2 = values + i * size;
            
            swap(p1, p2, size);
            v = (v == p1) ? p2 : ((v == p2) ? p1 : v);
            lt++; i++;
        }
        else if (cmp > 0) {
            uint8_t* p1 = values + i * size;
            uint8_t* p2 = values + gt * size;
            
            swap(p1, p2, size);
            v = (v == p1) ? p2 : ((v == p2) ? p1 : v);
            gt--;
        }
        else {
            i++;
        }
    }


    if (lt > 0) {
        __qsort(values, l, lt - 1, size, comp, swap);
    }
    __qsort(values, gt + 1, r, size, comp, swap);
}

void qsort(void* values, size_t count, size_t size, int (*comp)(const void*, const void*))
{
    if (count > 1 && size > 0 && comp && values) {
        swap_t swap;

        if (((uintptr_t)values & 0x03) == 0 && (size & 0x03) == 0) {
            swap = (swap_t)__swap_4b;
        }
        else {
            swap = (swap_t)__swap_1b;
        }

        __qsort(values, 0, count - 1, size, comp, swap);
    }
}
