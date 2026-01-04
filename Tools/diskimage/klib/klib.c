//
//  klib.c
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
#include <_time.h>
#include <ext/atomic.h>


const struct timespec TIMESPEC_ZERO = {0l, 0l};
const struct timespec TIMESPEC_INF = {LONG_MAX, NSEC_PER_SEC-1l};


////////////////////////////////////////////////////////////////////////////////

#include <kern/try.h>
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

//XXX not actually atomic for now. Note that the winnt.h InterlockedAdd() & co
// functions return the NEW value and not the old value as needed here.
void atomic_int_store(volatile atomic_int* _Nonnull p, int val)
{
    *p = val;
}

int atomic_int_load(volatile atomic_int* _Nonnull p)
{
    return *p;
}

int atomic_int_fetch_add(volatile atomic_int* _Nonnull p, int op)
{
    int old_val = *p;

    *p += op;
    return old_val;
}

int atomic_int_fetch_sub(volatile atomic_int* _Nonnull p, int op)
{
    int old_val = *p;

    *p -= op;
    return old_val;
}
