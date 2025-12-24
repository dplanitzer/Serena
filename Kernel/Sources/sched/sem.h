//
//  sem.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _SEM_H
#define _SEM_H 1

#include <kern/try.h>
#include <kern/types.h>
#include <sched/waitqueue.h>


// A (counting) semaphore
// Note: Interruptable
typedef struct sem {
    volatile int        value;
    struct waitqueue    wq;
} sem_t;


// Initializes a new semaphore with 'value' permits
extern void sem_init(sem_t* _Nonnull self, int value);

// Deinitializes the semaphore.
extern void sem_deinit(sem_t* _Nonnull self);


#define sem_relinquish(__self) \
sem_relinquish_multiple(__self, 1)

extern void sem_relinquish_multiple(sem_t* _Nonnull self, int npermits);

// Releases one permit to the semaphore from an interrupt context.
extern void sem_relinquish_irq(sem_t* _Nonnull self);


// Blocks the caller until the semaphore has at least one permit available or
// the wait has timed out. Note that this function may return EINTR which means
// that the sem_acquire() call is happening in the context of a system
// call that should be aborted.
#define sem_acquire(__self, __deadline) \
sem_acquire_multiple(__self, 1, __deadline)

extern errno_t sem_acquire_multiple(sem_t* _Nonnull self, int npermits, const struct timespec* _Nonnull deadline);
extern errno_t sem_acquireall(sem_t* _Nonnull self, const struct timespec* _Nonnull deadline, int* _Nonnull pOutPermitCount);


#define sem_tryacquire(__self) \
sem_tryacquire_multiple(__self, 1)

extern bool sem_tryacquire_multiple(sem_t* _Nonnull self, int npermits);
extern int sem_tryacquireall(sem_t* _Nonnull self);

#endif /* _SEM_H */
