//
//  Semaphore.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Semaphore_h
#define Semaphore_h

#include "Foundation.h"
#include "List.h"
#include "MonotonicClock.h"


// A binary semaphore
typedef struct _BinarySemaphore {
    volatile UInt   value;
    List            wait_queue;
} BinarySemaphore;


// Initializes a new binary semaphore. If 'isAvailable' is true then the semaphore
// starts out in released state.
extern void BinarySemaphore_Init(BinarySemaphore* _Nonnull pSema, Bool isAvailable);

// Deinitializes a binary semaphore.
extern void BinarySemaphore_Deinit(BinarySemaphore* _Nonnull pSema);

// Allocates a new binary semaphore.
extern BinarySemaphore* _Nullable BinarySemaphore_Create(Bool isAvailable);

// Deallocates a binary semaphore.
extern void BinarySemaphore_Destroy(BinarySemaphore* _Nullable pSema);

// Acquires the binary semaphore. Blocks the caller until the semaphore becomes available.
extern ErrorCode BinarySemaphore_Acquire(BinarySemaphore* _Nonnull pSema, TimeInterval deadline);

// Try to acquire the semaphore. Returns true on success and false otherwise. Never blocks.
extern Bool BinarySemaphore_TryAcquire(BinarySemaphore* _Nonnull sema);

// Releases the semaphore.
extern void BinarySemaphore_Release(BinarySemaphore* _Nonnull pSema);




// A counting semaphore
typedef struct _Semaphore {
    volatile Int    value;
    List            wait_queue;
} Semaphore;


extern Semaphore* _Nullable Semaphore_Create(Int value);
extern void Semaphore_Destroy(Semaphore* _Nullable pSemaphore);

// Initializes a new semaphore with 'value' permits
extern void Semaphore_Init(Semaphore* _Nonnull pSemaphore, Int value);
extern void Semaphore_Deinit(Semaphore* _Nonnull pSemaphore);

extern void Semaphore_Release(Semaphore* _Nonnull pSemaphore);
extern void Semaphore_ReleaseMultiple(Semaphore* _Nonnull sema, Int npermits);

extern ErrorCode Semaphore_Acquire(Semaphore* _Nonnull pSemaphore, TimeInterval deadline);
extern ErrorCode Semaphore_AcquireMultiple(Semaphore* _Nonnull sema, Int npermits, TimeInterval deadline);
extern ErrorCode Semaphore_AcquireAll(Semaphore* _Nonnull pSemaphore, TimeInterval deadline, Int* _Nonnull pOutPermitCount);

extern Bool Semaphore_TryAcquire(Semaphore* _Nonnull pSemaphore);
extern Bool Semaphore_TryAcquireMultiple(Semaphore* _Nonnull pSemaphore, Int npermits);
extern Int Semaphore_TryAcquireAll(Semaphore* _Nonnull pSemaphore);

#endif /* Semaphore_h */
