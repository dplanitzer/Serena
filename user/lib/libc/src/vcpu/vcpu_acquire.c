//
//  vcpu_acquire.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <stdlib.h>
#include <serena/spinlock.h>
#include <kpi/syscall.h>


static _Noreturn void __vcpu_start(vcpu_t self)
{
    self->func(self->arg);

    // Make sure that we clean up the user space side of things before we let
    // the vcpu relinquish for good.
    __vcpu_relinquish(self);
    /* NOT REACHED */
}

vcpu_t _Nullable vcpu_acquire(const vcpu_attr_t* _Nonnull attr)
{
    vcpu_t self = calloc(1, sizeof(struct vcpu));
    vcpu_attr_t sys_attr;

    if (self == NULL) {
        return NULL;
    }

    self->group_id = attr->group_id;
    self->func = attr->func;
    self->arg = attr->arg;


    sys_attr.func = (vcpu_func_t)__vcpu_start;
    sys_attr.arg = self;
    sys_attr.stack_size = attr->stack_size;
    sys_attr.group_id = attr->group_id;
    sys_attr.policy = attr->policy;
    sys_attr.flags = attr->flags & ~VCPU_ACQUIRE_RESUMED;

    if (_syscall(SC_vcpu_acquire, &sys_attr, (intptr_t)self, &self->id) < 0) {
        free(self);
        return NULL;
    }

    spin_lock(&__g_lock);
    deque_add_last(&__g_all_vcpus, &self->qe);
    spin_unlock(&__g_lock);


    if ((attr->flags & VCPU_ACQUIRE_RESUMED) == VCPU_ACQUIRE_RESUMED) {
        vcpu_resume(self);
    }
    return self;
}
