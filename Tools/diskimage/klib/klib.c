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
#include <time.h>

#define ONE_SECOND_IN_NANOS (1000l * 1000l * 1000l)


const struct timespec TIMESPEC_ZERO = {0l, 0l};
const struct timespec TIMESPEC_INF = {LONG_MAX, ONE_SECOND_IN_NANOS};
const struct timespec TIMESPEC_NEGINF = {LONG_MIN, ONE_SECOND_IN_NANOS};


////////////////////////////////////////////////////////////////////////////////

#include "Atomic.h"

// Atomically assign 'newValue' to the atomic bool stored in the given memory
// location and returns the previous value.
AtomicBool AtomicBool_Set(volatile AtomicBool* _Nonnull pValue, bool newValue)
{
    AtomicBool oldValue = *pValue;
    *pValue = newValue;
    return oldValue;
}

// Atomically adds the 'increment' value to the integer stored in the given
// memory location and returns the new value.
AtomicInt AtomicInt_Add(volatile AtomicInt* _Nonnull pValue, int increment)
{
    *pValue += increment;
    return *pValue;
}

// Atomically subtracts the 'decrement' value from the integer stored in the given
// memory location and returns the new value.
AtomicInt AtomicInt_Subtract(volatile AtomicInt* _Nonnull pValue, int decrement)
{
    *pValue -= decrement;
    return *pValue;
}


////////////////////////////////////////////////////////////////////////////////

#include <kern/errno.h>
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

#include <kern/string.h>

ssize_t String_Length(const char* _Nonnull pStr)
{
    return strlen(pStr);
}

ssize_t String_LengthUpTo(const char* _Nonnull pStr, ssize_t strsz)
{
    ssize_t len = 0;

    while (*pStr++ != '\0' && len < strsz) {
        len++;
    }

    return len;
}

char* _Nonnull String_CopyUpTo(char* _Nonnull pDst, const char* _Nonnull pSrc, ssize_t count)
{
    while (*pSrc != '\0' && count > 0) {
        *pDst++ = *pSrc++;
        count--;
    }
    if (count > 0) {
        *pDst++ = '\0';
    }

    return pDst;
}
