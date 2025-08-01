//
//  klib_Atomic.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef klib_Atomic_h
#define klib_Atomic_h

#include <kern/types.h>
#include <Windows.h>


// Atomic bool

typedef CHAR    AtomicBool;

// Simple assignment and get are naturally atomic because they compile to a single
// instruction anyway.

// Atomically assign 'newValue' to the atomic bool stored in the given memory
// location and returns the previous value.
#define AtomicBool_Set(__pv, __nv) \
InterlockedExchange8(__pv, __nv)



// Atomic int

typedef LONG    AtomicInt;


// Atomically adds the 'increment' value to the integer stored in the given
// memory location and returns the new value.
#define AtomicInt_Add(__pv, __inc) \
InterlockedAdd(__pv, __inc)

// Atomically subtracts the 'decrement' value from the integer stored in the given
// memory location and returns the new value.
#define AtomicInt_Subtract(__pv, __dec) \
InterlockedAdd(__pv, -(__dec))

// Atomically increments the integer stored in the given memory location by one
// and returns the new value.
#define AtomicInt_Increment(__pv) \
InterlockedIncrement(__pv)

// Atomically decrements the integer stored in the given memory location by one
// and returns the new value.
#define AtomicInt_Decrement(__pv) \
InterlockedDecrement(__pv)

#endif /* klib_Atomic_h */
