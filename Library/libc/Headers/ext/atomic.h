//
//  ext/atomic.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 6/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_ATOMIC_H
#define _EXT_ATOMIC_H 1

#include <_cmndef.h>
#include <stdbool.h>

__CPP_BEGIN

typedef unsigned char atomic_flag;

#define ATOMIC_FLAG_INIT    0


extern bool atomic_flag_test_and_set(volatile atomic_flag* _Nonnull flag);
extern void atomic_flag_clear(volatile atomic_flag* _Nonnull flag);


typedef bool            atomic_bool;
typedef int             atomic_int;

// This is actually not atomic. It's purely used to initialize an atomic value
// before any other thread-of-execution can access it.
#define atomic_init(__ptr_to_atomic_typ, __val) \
*(__ptr_to_atomic_typ) = (__val)

// Atomically set the value of 'p' to 'val'.
extern void atomic_int_store(volatile atomic_int* _Nonnull p, int val);

// Atomically reads the current value of 'p'.
extern int atomic_int_load(volatile atomic_int* _Nonnull p);

#if defined(__KERNEL__)
// Atomically adds 'op' to 'p'. Does not detect overflow. Instead the value will
// wrap around. Returns the old value.
extern int atomic_int_fetch_add(volatile atomic_int* _Nonnull p, int op);

// Atomically subtracts 'op' from 'p'. Does not detect overflow. Instead the
// value will wrap around. Returns the old value.
extern int atomic_int_fetch_sub(volatile atomic_int* _Nonnull p, int op);
#endif

__CPP_END

#endif /* _EXT_ATOMIC_H */
