//
//  sys/spinlock.h
//  libc
//
//  Created by Dietmar Planitzer on 6/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SPINLOCK_H
#define _SYS_SPINLOCK_H 1

#include <_cmndef.h>
#include <arch/_null.h>
#include <ext/atomic.h>

__CPP_BEGIN

typedef struct spinlock {
    atomic_flag lock;
    char        pad[3];
} spinlock_t;

#define SPINLOCK_INIT   (spinlock_t){0,0,0,0}


// Blocks the caller until the spin lock has been acquired.
extern void spin_lock(volatile spinlock_t* _Nonnull l);

// Attempts to acquire the given lock. True is return if the lock has been
// successfully acquired and false otherwise.
extern bool spin_trylock(volatile spinlock_t* _Nonnull l);

// Unlocks the spin lock.
extern void spin_unlock(volatile spinlock_t* _Nonnull l);

__CPP_END

#endif /* _SYS_SPINLOCK_H */
