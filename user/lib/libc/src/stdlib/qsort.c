//
//  qsort.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <_sortsearch.h>
#include <stddef.h>
#include <stdint.h>

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

static void __swap_4b_exactly(uint32_t* _Nonnull p1, uint32_t* _Nonnull p2, size_t size) 
{
    const uint32_t tmp = *p1;

    *p1 = *p2;
    *p2 = tmp;
}


// Based on "Basic Quicksort" from the book Algorithms 4th Edition by Robert Sedgewick and Kevin Wayne, pages 299ff.
static int __partition(uint8_t* values, long lo, long hi, size_t size, compare_to_t comp, swap_t swap)
{
    int i = lo, j = hi + 1;
    uint8_t* v = values + lo * size;

    while (1) {
        while (comp(values + (++i) * size, v) < 0) if (i == hi) break;
        while (comp(v, values + (--j) * size) < 0) if (j == lo) break;
        
        if (i >= j) break;
        
        uint8_t* ip = values + i * size;
        uint8_t* jp = values + j * size;
        swap(ip, jp, size);
    }
    
    uint8_t* p1 = values + lo * size;
    uint8_t* p2 = values + j * size;
    swap(p1, p2, size);

    return j;
}

static void __qsort(uint8_t* values, long lo, long hi, size_t size, compare_to_t comp, swap_t swap) 
{
    if (hi <= lo) {
        return;
    }
    
    const int j = __partition(values, lo, hi, size, comp, swap);
    __qsort(values, lo, j - 1, size, comp, swap);
    __qsort(values, j + 1, hi, size, comp, swap);
}


void qsort(void* values, size_t count, size_t size, int (*comp)(const void*, const void*))
{
    if (count > 1 && size > 0 && comp && values) {
        swap_t swap;

        if (((uintptr_t)values & 0x03) == 0 && (size & 0x03) == 0) {
            swap = (size == 4) ? (swap_t)__swap_4b_exactly : (swap_t)__swap_4b;
        }
        else {
            swap = (swap_t)__swap_1b;
        }

        __qsort(values, 0, count - 1, size, comp, swap);
    }
}
