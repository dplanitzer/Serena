//
//  mtx.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _MTX_H
#define _MTX_H 1

#include <kern/try.h>
#include <kern/types.h>
#include <sched/waitqueue.h>

struct vcpu;


typedef struct mtx {
    volatile uint32_t       value;
    struct waitqueue        wq;
    struct vcpu* _Nullable  owner;  // Pointer to the VP that is currently holding the lock
} mtx_t;


// Initializes a new mutex.
extern void mtx_init(mtx_t* _Nonnull self);

// Deinitializes a mutex.
extern void mtx_deinit(mtx_t* _Nonnull self);


// Attempts to acquire the given mutex. True is return if the lock has been
// successfully acquired and false otherwise.
extern bool mtx_trylock(mtx_t* _Nonnull self);

// Blocks the caller until the mutex can be taken successfully.
extern void mtx_lock(mtx_t* _Nonnull self);

// Unlocks the mutex. A call to fatalError() is triggered if the caller does not
// hold the mutex.
extern void mtx_unlock(mtx_t* _Nonnull self);

// Unlocks the mutex and then enters the wait queue 'wq'. The unlock and entering
// the wait are done atomically.
extern errno_t mtx_unlock_then_wait(mtx_t* _Nonnull self, struct waitqueue* _Nonnull wq);

// Returns the virtual processor that is currently holding the mutex. NULL is
// returned if none is holding the lock.
extern struct vcpu* _Nullable _Nullable mtx_owner(mtx_t* _Nonnull self);

#endif /* _MTX_H */
