//
//  Semaphore.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Semaphore_h
#define Semaphore_h

#include <kern/errno.h>
#include <kern/types.h>
#include <klib/List.h>


// A (counting) semaphore
typedef struct Semaphore {
    volatile int    value;
    List            wait_queue;
} Semaphore;


// Initializes a new semaphore with 'value' permits
extern void Semaphore_Init(Semaphore* _Nonnull pSemaphore, int value);

// Deinitializes the semaphore. All virtual processors that are still waiting
// for permits on this semaphore are woken up with an EINTR error.
extern void Semaphore_Deinit(Semaphore* _Nonnull pSemaphore);


#define Semaphore_Relinquish(__self) \
Semaphore_RelinquishMultiple(__self, 1)

extern void Semaphore_RelinquishMultiple(Semaphore* _Nonnull sema, int npermits);

// Releases one permit to the semaphore from an interrupt context.
extern void Semaphore_RelinquishFromInterruptContext(Semaphore* _Nonnull sema);


// Blocks the caller until the semaphore has at least one permit available or
// the wait has timed out. Note that this function may return EINTR which means
// that the Semaphore_Acquire() call is happening in the context of a system
// call that should be aborted.
#define Semaphore_Acquire(__self, __deadline) \
Semaphore_AcquireMultiple(__self, 1, __deadline)

extern errno_t Semaphore_AcquireMultiple(Semaphore* _Nonnull sema, int npermits, struct timespec deadline);
extern errno_t Semaphore_AcquireAll(Semaphore* _Nonnull pSemaphore, struct timespec deadline, int* _Nonnull pOutPermitCount);


#define Semaphore_TryAcquire(__self) \
Semaphore_TryAcquireMultiple(__self, 1)

extern bool Semaphore_TryAcquireMultiple(Semaphore* _Nonnull pSemaphore, int npermits);
extern int Semaphore_TryAcquireAll(Semaphore* _Nonnull pSemaphore);

#endif /* Semaphore_h */
