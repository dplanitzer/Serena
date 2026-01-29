//
//  kernlib.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Assert.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ext/atomic.h>
#include <kpi/_time.h>


const struct timespec TIMESPEC_ZERO = {0l, 0l};
const struct timespec TIMESPEC_INF = {LONG_MAX, NSEC_PER_SEC-1l};


////////////////////////////////////////////////////////////////////////////////

#include <ext/try.h>
#include <kern/kalloc.h>
#include <stdlib.h>


// Allocates memory from the kernel heap. Returns NULL if the memory could not be
// allocated. 'options' is a combination of the HEAP_ALLOC_OPTION_XXX flags.
errno_t kalloc_options(ssize_t nbytes, unsigned int options, void* _Nullable * _Nonnull pOutPtr)
{
    if ((options & KALLOC_OPTION_CLEAR) != 0) {
        *pOutPtr = calloc(1, nbytes);
    }
    else {
        *pOutPtr = malloc(nbytes);
    }

    return (*pOutPtr) ? EOK : ENOMEM;
}

// Frees kernel memory allocated with the kalloc() function.
void kfree(void* _Nullable ptr)
{
    free(ptr);
}


////////////////////////////////////////////////////////////////////////////////
#if 0
//XXX this is just a reference and not suitable for production use since its not
// actually atomic.
void atomic_int_store(volatile atomic_int* _Nonnull p, int val)
{
    p->value = val;
}

int atomic_int_load(volatile atomic_int* _Nonnull p)
{
    return p->value;
}


int atomic_int_exchange(volatile atomic_int* _Nonnull p, int op)
{
    const int old_val = p->value;

    p->value = op;
    return old_val;
}

bool atomic_int_compare_exchange_strong(volatile atomic_int* _Nonnull p, volatile atomic_int* _Nonnull expected, int desired)
{
    if (p->value == expected->value) {
        p->value = desired;
        return true;
    }
    else {
        expected->value = p->value;
        return false;
    }
}

int atomic_int_fetch_add(volatile atomic_int* _Nonnull p, int op)
{
    const int old_val = p->value;

    p->value += op;
    return old_val;
}

int atomic_int_fetch_sub(volatile atomic_int* _Nonnull p, int op)
{
    const int old_val = p->value;

    p->value -= op;
    return old_val;
}

int atomic_int_fetch_or(volatile atomic_int* _Nonnull p, int op)
{
    const int old_val = p->value;

    p->value |= op;
    return old_val;
}

int atomic_int_fetch_xor(volatile atomic_int* _Nonnull p, int op)
{
    const int old_val = p->value;

    p->value ^= op;
    return old_val;
}

int atomic_int_fetch_and(volatile atomic_int* _Nonnull p, int op)
{
    const int old_val = p->value;

    p->value &= op;
    return old_val;
}
#endif
