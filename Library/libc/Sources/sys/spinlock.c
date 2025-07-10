//
//  sys/spinlock.c
//  libc
//
//  Created by Dietmar Planitzer on 6/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/spinlock.h>
#include <sys/vcpu.h>


void spin_lock(volatile spinlock_t* _Nonnull l)
{
    while(atomic_flag_test_and_set(&l->lock)) {
        vcpu_yield();
    }
}

bool spin_trylock(volatile spinlock_t* _Nonnull l)
{
    return !atomic_flag_test_and_set(&l->lock);
}

void spin_unlock(volatile spinlock_t* _Nonnull l)
{
    atomic_flag_clear(&l->lock);
}
