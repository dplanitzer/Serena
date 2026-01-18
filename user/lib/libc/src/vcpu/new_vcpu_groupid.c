//
//  new_vcpu_groupid.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <sys/spinlock.h>


vcpuid_t new_vcpu_groupid(void)
{
    static spinlock_t l;
    static vcpuid_t id;

    spin_lock(&l);
    const newid = ++id;
    spin_unlock(&l);

    return newid;
}
