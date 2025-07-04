//
//  Semaphore.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Semaphore_h
#define Semaphore_h

#include <dispatcher/WaitQueue.h>
#include <kern/errno.h>
#include <kern/types.h>


// A (counting) semaphore
// Note: Interruptable
typedef struct Semaphore {
    volatile int    value;
    WaitQueue       wq;
} Semaphore;


// Initializes a new semaphore with 'value' permits
extern void Semaphore_Init(Semaphore* _Nonnull self, int value);

// Deinitializes the semaphore.
extern void Semaphore_Deinit(Semaphore* _Nonnull self);


#define Semaphore_Relinquish(__self) \
Semaphore_RelinquishMultiple(__self, 1)

extern void Semaphore_RelinquishMultiple(Semaphore* _Nonnull self, int npermits);

// Releases one permit to the semaphore from an interrupt context.
extern void Semaphore_RelinquishFromInterrupt(Semaphore* _Nonnull self);


// Blocks the caller until the semaphore has at least one permit available or
// the wait has timed out. Note that this function may return EINTR which means
// that the Semaphore_Acquire() call is happening in the context of a system
// call that should be aborted.
#define Semaphore_Acquire(__self, __deadline) \
Semaphore_AcquireMultiple(__self, 1, __deadline)

extern errno_t Semaphore_AcquireMultiple(Semaphore* _Nonnull self, int npermits, const struct timespec* _Nonnull deadline);
extern errno_t Semaphore_AcquireAll(Semaphore* _Nonnull self, const struct timespec* _Nonnull deadline, int* _Nonnull pOutPermitCount);


#define Semaphore_TryAcquire(__self) \
Semaphore_TryAcquireMultiple(__self, 1)

extern bool Semaphore_TryAcquireMultiple(Semaphore* _Nonnull self, int npermits);
extern int Semaphore_TryAcquireAll(Semaphore* _Nonnull self);

#endif /* Semaphore_h */
