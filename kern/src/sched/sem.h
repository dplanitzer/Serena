//
//  sem.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _SEM_H
#define _SEM_H 1

#include <stdbool.h>
#include <ext/try.h>
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

// @IRQ Context Safe
extern void sem_post(sem_t* _Nonnull self);

extern void sem_wait(sem_t* _Nonnull self);
extern errno_t sem_timedwait(sem_t* _Nonnull self, const nanotime_t* _Nonnull deadline);
extern bool sem_trywait(sem_t* _Nonnull self);

#endif /* _SEM_H */
