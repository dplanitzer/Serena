//
//  USemaphore.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef USemaphore_h
#define USemaphore_h

#include <klib/klib.h>
#include <kobj/Object.h>
#include <dispatcher/Semaphore.h>


OPEN_CLASS_WITH_REF(USemaphore, Object,
    Semaphore    sema;
);
typedef struct USemaphoreMethodTable {
    ObjectMethodTable   super;
} USemaphoreMethodTable;


// Creates a semaphore suitable for use by userspace code. 'npermits' is the initial
// permit count of the semaphore.
extern errno_t USemaphore_Create(int npermits, USemaphoreRef _Nullable * _Nonnull pOutSelf);


// Releases 'npermits' permits to the semaphore.
#define USemaphore_Relinquish(__self, __npermits) \
Semaphore_RelinquishMultiple(&(__self)->sema, __npermits)

// Blocks the caller until 'npermits' can be successfully acquired from the given
// semaphore. Returns EOK on success and ETIMEOUT if the permits could not be
// acquired before 'deadline'.
#define USemaphore_Acquire(__self, __npermits, __deadline) \
Semaphore_AcquireMultiple(&(__self)->sema, __npermits, __deadline)

// Tries to acquire 'npermits' from the given semaphore. Returns true on success
// and false otherwise. This function does not block the caller.
#define USemaphore_TryAcquire(__self, __npermits) \
Semaphore_TryAcquireMultiple(&(__self)->sema, __npermits)

#endif /* USemaphore_h */
