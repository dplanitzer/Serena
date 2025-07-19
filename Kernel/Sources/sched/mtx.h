//
//  mtx.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Mtx_h
#define Mtx_h

#include <kern/errno.h>
#include <kern/types.h>
#include <sched/waitqueue.h>


typedef struct mtx {
    volatile uint32_t   value;
    struct waitqueue    wq;
    int                 owner_vpid;     // ID of the VP that is currently holding the lock
} mtx_t;


// Initializes a new lock appropriately for use in the kernel. This means that:
// - ownership tracking is turned on and violations will trigger a fatal error condition
extern void mtx_init(mtx_t* _Nonnull self);

// Deinitializes a lock.
extern void mtx_deinit(mtx_t* _Nonnull self);


// Attempts to acquire the given lock. True is return if the lock has been
// successfully acquired and false otherwise.
extern bool mtx_trylock(mtx_t* _Nonnull self);

// Blocks the caller until the lock can be taken successfully.
extern errno_t mtx_lock(mtx_t* _Nonnull self);

// Unlocks the lock. A call to fatalError() is triggered if the caller does not
// hold the lock. Otherwise returns EOK.
extern errno_t mtx_unlock(mtx_t* _Nonnull self);


// Returns the ID of the virtual processor that is currently holding the lock.
// Zero is returned if none is holding the lock.
extern int mtx_ownerid(mtx_t* _Nonnull self);

#endif /* Mtx_h */
