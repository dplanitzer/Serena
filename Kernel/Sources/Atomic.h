//
//  Atomic.h
//  Apollo
//
//  Created by Dietmar Planitzer on 3/15/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Atomic_h
#define Atomic_h

#include "Foundation.h"


// Atomic Bool

// Nothing special needed because all operations currently compile to a single instruction anyway
typedef Bool    AtomicBool;


// Atomic Int

typedef Int     AtomicInt;


// Atomically increments the integer stored in the given memory location by one
// and returns the previous value.
extern AtomicInt AtomicInt_Increment(volatile AtomicInt* _Nonnull pValue);

// Atomically decrements the integer stored in the given memory location by one
// and returns the previous value.
extern AtomicInt AtomicInt_Decrement(volatile AtomicInt* _Nonnull pValue);

// Atomically adds the 'increment' value to the integer stored in the given
// memory location and returns the previous value.
extern AtomicInt AtomicInt_Add(volatile AtomicInt* _Nonnull pValue, Int increment);

#endif /* Atomic_h */
