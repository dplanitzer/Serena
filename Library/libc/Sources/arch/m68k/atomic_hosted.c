//
//  atomic_hosted.c
//  libc
//
//  Created by Dietmar Planitzer on 1/3/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/atomic.h>
#include <sys/vcpu.h>


static void __lock(volatile atomic_int* _Nonnull p)
{
    while(atomic_flag_test_and_set(&p->lock)) {
        vcpu_yield();
    }
}

#define __unlock(__p) \
atomic_flag_clear(&(__p)->lock)


int atomic_int_exchange(volatile atomic_int* _Nonnull p, int op)
{
    __lock(p);
    const int old_val = p->value;
    p->value = op;
    __unlock(p);

    return old_val;
}

bool atomic_int_compare_exchange_strong(volatile atomic_int* _Nonnull p, volatile atomic_int* _Nonnull expected, int desired)
{
    bool r;
    volatile atomic_int* l1 = (p <= expected) ? p : expected;
    volatile atomic_int* l2 = (p <= expected) ? expected : p;

    __lock(l1);
    __lock(l2);

    if (p->value == expected->value) {
        p->value = desired;
        r = true;
    }
    else {
        expected->value = p->value;
        r = false;
    }

    __unlock(l2);
    __unlock(l1);

    return r;
}

int atomic_int_fetch_add(volatile atomic_int* _Nonnull p, int op)
{
    __lock(p);
    const int old_val = p->value;
    p->value += op;
    __unlock(p);

    return old_val;
}

int atomic_int_fetch_sub(volatile atomic_int* _Nonnull p, int op)
{
    __lock(p);
    const int old_val = p->value;
    p->value -= op;
    __unlock(p);

    return old_val;
}

int atomic_int_fetch_or(volatile atomic_int* _Nonnull p, int op)
{
    __lock(p);
    const int old_val = p->value;
    p->value |= op;
    __unlock(p);

    return old_val;
}

int atomic_int_fetch_xor(volatile atomic_int* _Nonnull p, int op)
{
    __lock(p);
    const int old_val = p->value;
    p->value ^= op;
    __unlock(p);

    return old_val;
}

int atomic_int_fetch_and(volatile atomic_int* _Nonnull p, int op)
{
    __lock(p);
    const int old_val = p->value;
    p->value &= op;
    __unlock(p);

    return old_val;
}
