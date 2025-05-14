//
//  Atomic.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/15/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Atomic_h
#define Atomic_h

#include <kern/types.h>


// Atomic bool

typedef bool    AtomicBool;

// Simple assignment and get are naturally atomic because they compile to a single
// instruction anyway.

// Atomically assign 'newValue' to the atomic bool stored in the given memory
// location and returns the previous value.
extern AtomicBool AtomicBool_Set(volatile AtomicBool* _Nonnull pValue, bool newValue);



// Atomic int

typedef int     AtomicInt;


// Atomically adds the 'increment' value to the integer stored in the given
// memory location and returns the new value.
extern AtomicInt AtomicInt_Add(volatile AtomicInt* _Nonnull pValue, int increment);

// Atomically subtracts the 'decrement' value from the integer stored in the given
// memory location and returns the new value.
extern AtomicInt AtomicInt_Subtract(volatile AtomicInt* _Nonnull pValue, int decrement);

// Atomically increments the integer stored in the given memory location by one
// and returns the new value.
static inline AtomicInt AtomicInt_Increment(volatile AtomicInt* _Nonnull pValue)
{
    return AtomicInt_Add(pValue, 1);
}

// Atomically decrements the integer stored in the given memory location by one
// and returns the new value.
static inline AtomicInt AtomicInt_Decrement(volatile AtomicInt* _Nonnull pValue)
{
    return AtomicInt_Subtract(pValue, 1);
}

#endif /* Atomic_h */
